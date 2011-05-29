/*!
\author Rémi Attab
\license FreeBSD (see the LICENSE file)

 */


#include <dependency.h>

#include <types.h>
#include <helper.h>
#include "epoch.h"
#include "atomic.h"
#include "map.h"
#include "pmem.h"
#include "pstore.h"
#include "bits.h"

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


struct addr_info {
  pthread_mutex_t lock;
  uint32_t read_flags;
  uint32_t write_flags;  
  struct addr_info** addr_list;
  yarn_word_t write_buffer[];
};


// Contains all the dependency information for the speculative threads.
static struct yarn_map* g_dependency_map;

// Pool allocator for temp addr_info values.
static struct yarn_pmem* g_addr_info_alloc;

//! \todo Probably won't only hold epochs.
static struct yarn_pstore* g_epoch_store;



static inline size_t addr_info_size ();

static inline yarn_word_t get_epoch (yarn_word_t pool_id);
static inline yarn_word_t index_to_epoch_after (yarn_word_t base_epoch, 
						yarn_word_t index);
static inline yarn_word_t index_to_epoch_before (yarn_word_t base_epoch, 
						 yarn_word_t index);

static inline struct addr_info* acquire_map_addr_info (yarn_word_t pool_id, void* addr);
static inline void release_map_addr_info (struct addr_info* info);

static inline void dep_violation_check (yarn_word_t epoch, yarn_word_t read_flags);

static inline void store_to_wbuf (struct addr_info* info, yarn_word_t epoch, 
				  void* src, void* dest, size_t size);
static inline void load_from_wbuf (struct addr_info* info, yarn_word_t epoch, 
				   void* src, void* dest, size_t size); 






static bool addr_info_construct(void* data) { 
  struct addr_info* info = (struct addr_info*) data;

  int ret = pthread_mutex_init(&info->lock, NULL);
  if (!ret) goto mutex_error;

  info->addr_list = (struct addr_info**) (info->write_buffer + YARN_EPOCH_MAX);
  for (size_t i = 0; i < YARN_EPOCH_MAX; ++i) {
    info->addr_list[i] = NULL;
  }

  return true;

  // pthread_mutex_destroy(&info->lock);
 mutex_error:
  perror(__FUNCTION__);
  return false;
}

static void addr_info_destruct(void* data) {
  struct addr_info* info = (struct addr_info*) data;
  pthread_mutex_destroy(&info->lock);
  free(info->write_buffer);
  free(info->addr_list);
}




bool yarn_dep_global_init (size_t ws_size) {
  g_dependency_map = yarn_map_init(ws_size);
  if (!g_dependency_map) goto map_error;

  g_addr_info_alloc = yarn_pmem_init(addr_info_size(), 
				     addr_info_construct, 
				     addr_info_destruct);
  if (!g_addr_info_alloc) goto allocator_error;

  g_epoch_store = yarn_pstore_init();
  if (!g_epoch_store) goto epoch_store_error;

  return true;

  // yarn_pstore_destroy(g_epoch_store);
 epoch_store_error:
  yarn_pmem_destroy(g_addr_info_alloc);
 allocator_error:
  yarn_map_destroy(g_dependency_map);
 map_error:
  perror(__FUNCTION__);
  return false;
}

void yarn_dep_global_destroy (void) {
  yarn_map_destroy(g_dependency_map);
  yarn_pmem_destroy(g_addr_info_alloc);

  for (yarn_word_t pool_id = 0; pool_id < yarn_tpool_size(); ++pool_id) {
    yarn_word_t* p_epoch = (yarn_word_t*) yarn_pstore_load(g_epoch_store, pool_id);
    if (p_epoch != NULL) {
      free(p_epoch);
    }
  }
  yarn_pstore_destroy(g_epoch_store);
}




bool yarn_dep_thread_init (yarn_word_t pool_id, yarn_word_t epoch) {
  yarn_word_t* p_epoch = yarn_pstore_load(g_epoch_store, pool_id);
  
  if (p_epoch == NULL) {
    p_epoch = (yarn_word_t*) malloc(sizeof(yarn_word_t));
    if (!p_epoch) goto alloc_error;

    yarn_pstore_store(g_epoch_store, pool_id, p_epoch);
  }

  *p_epoch = epoch;
  return true;

 alloc_error:
  perror(__FUNCTION__);
  return false;
}

void yarn_dep_thread_destroy (yarn_word_t pool_id) {
  (void) pool_id;
  // g_epoch_store free is handled by global_destroy.
}



bool yarn_dep_store (yarn_word_t pool_id, void* src, void* dest, size_t size) {
  // alignment check
  assert(sizeof(yarn_word_t) == 4 ? 
	 ((uintptr_t)src & ~3) == (uintptr_t)src : 
	 ((uintptr_t)src & ~7) == (uintptr_t)src);
  // we don't support this for the moment.
  assert(size <= sizeof(yarn_word_t));
  (void) size;

  yarn_word_t read_flags;
  const yarn_word_t epoch = get_epoch(pool_id);

  // Buffer the write.
  {
    struct addr_info* info = acquire_map_addr_info(pool_id, dest);
    if (!info) goto acquire_error;

    read_flags = info->read_flags;
    store_to_wbuf(info, epoch, src, dest, size);

    release_map_addr_info(info);
  }

  dep_violation_check (epoch, read_flags);

  return true;

 acquire_error:
  perror(__FUNCTION__);
  return false;
}


bool yarn_dep_load (yarn_word_t pool_id, void* src, void* dest, size_t size) {
  // alignment check
  assert(sizeof(yarn_word_t) == 4 ? 
	 ((uintptr_t)src & ~3) == (uintptr_t)src : 
	 ((uintptr_t)src & ~7) == (uintptr_t)src);
  // we don't support this for the moment.
  assert(size <= sizeof(yarn_word_t));
  (void) size;

  const yarn_word_t epoch = get_epoch(pool_id);

  {
    struct addr_info* info = acquire_map_addr_info(pool_id, src);
    if (!info) goto acquire_error;

    load_from_wbuf(info, epoch, src, dest, size);

    release_map_addr_info(info);
  }
 
  return true;
  
 acquire_error:
  perror(__FUNCTION__);
  return false;
}



static inline size_t addr_info_size () {
  size_t size = sizeof(struct addr_info);
  size += sizeof(yarn_word_t) * YARN_EPOCH_MAX;
  size += sizeof(struct addr_info*) * YARN_EPOCH_MAX;
  return size;
}


static inline yarn_word_t get_epoch(yarn_word_t pool_id) {
  return *((yarn_word_t*)yarn_pstore_load(g_epoch_store, pool_id));
}


static inline yarn_word_t index_to_epoch_after (yarn_word_t base_epoch, 
						yarn_word_t index) 
{
  const yarn_word_t base_index = YARN_BIT_INDEX(base_epoch);
  
  if (base_index <= index) {
    return base_epoch + (index - base_index);
  }
  else {
    return base_epoch + (YARN_EPOCH_MAX - base_index) + index;
  }
}


static inline yarn_word_t index_to_epoch_before (yarn_word_t base_epoch, 
						 yarn_word_t index) 
{
  const yarn_word_t base_index = YARN_BIT_INDEX(base_epoch);
  
  if (base_index >= index) {
    return base_epoch - (base_index - index);
  }
  else {
    return base_epoch - base_index - (YARN_EPOCH_MAX - index);
  }
}



static inline struct addr_info* acquire_map_addr_info (yarn_word_t pool_id, void* addr) {

  struct addr_info* tmp_info = yarn_pmem_alloc(g_addr_info_alloc, pool_id);
  if (!tmp_info) goto alloc_error;

  YARN_CHECK_RET0(pthread_mutex_lock(&tmp_info->lock));
  
  struct addr_info* info = (struct addr_info*) 
      yarn_map_probe(g_dependency_map, (uintptr_t)addr, tmp_info);
  
  if (info != tmp_info) {
    YARN_CHECK_RET0(pthread_mutex_unlock(&tmp_info->lock));
    yarn_pmem_free(g_addr_info_alloc, pool_id, tmp_info);
    tmp_info = NULL;
    YARN_CHECK_RET0(pthread_mutex_lock(&info->lock));
  }

  return info;

 alloc_error:
  perror(__FUNCTION__);
  return NULL;
}


static inline void release_map_addr_info (struct addr_info* info) {
  YARN_CHECK_RET0(pthread_mutex_unlock(&info->lock));  
}



static inline void store_to_wbuf (struct addr_info* info, 
				  yarn_word_t epoch, 
				  void* src, 
				  void* dest,
				  size_t size) 
{
  (void) dest;

  const yarn_word_t epoch_index = YARN_BIT_INDEX(epoch);

  info->write_flags = YARN_BIT_SET(info->write_flags, epoch);

  memcpy(&info->write_buffer[epoch_index], src, size);
}

static inline void load_from_wbuf (struct addr_info* info, 
				   yarn_word_t epoch, 
				   void* src, 
				   void* dest, 
				   size_t size)
{
  info->read_flags = YARN_BIT_SET(info->read_flags, epoch);

  const yarn_word_t old_first = yarn_epoch_first();

  const yarn_word_t range_mask = yarn_bit_mask_range(old_first, epoch);
  const yarn_word_t rollback_mask = ~yarn_epoch_rollback_flags();

  yarn_word_t write_flags = info->write_flags;
  write_flags &= range_mask;
  write_flags &= rollback_mask;

  // Check in wbuf for a value to read.
  if (write_flags != 0) {
    const yarn_word_t read_index = yarn_bit_log2(write_flags);
    const yarn_word_t read_epoch = index_to_epoch_before(epoch, read_index);
    memcpy(dest, &info->write_buffer[read_index], size);

    const yarn_word_t new_first = yarn_epoch_first();
    // If the value was comitted while we were reading, go to memory.
    if (new_first <= read_epoch) {
      return;
    }
  }

  // No values in the buffer, read straight from memory.
  memcpy(dest, src, size);
}


static inline void dep_violation_check (yarn_word_t epoch, yarn_word_t read_flags) {

  const yarn_word_t last_epoch = yarn_epoch_last();
  if (epoch >= last_epoch) {
    return;
  }

  const yarn_word_t rollback_mask = ~yarn_epoch_rollback_flags();  
  const yarn_word_t range_mask = yarn_bit_mask_range(epoch+1, last_epoch);
  
  read_flags &= range_mask;
  read_flags &= rollback_mask;

  if (read_flags == 0) {
    return;
  }

  yarn_word_t rollback_index = yarn_bit_trailing_zeros(read_flags)+1;
  yarn_word_t rollback_epoch = index_to_epoch_after(epoch, rollback_index);
  yarn_epoch_do_rollback(rollback_epoch);

}
