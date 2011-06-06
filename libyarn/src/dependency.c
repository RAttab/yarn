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
#include "timestamp.h"

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


struct addr_info {
  void* addr;
  yarn_word_t last_commit;
  yarn_word_t read_flags;
  yarn_word_t write_flags;

  pthread_mutex_t lock;

  struct addr_info** info_list;
  yarn_word_t write_buffer[];
};


// Contains all the dependency information for the speculative threads.
static struct yarn_map* g_dependency_map;

// Pool allocator for temp addr_info values.
static struct yarn_pmem* g_addr_info_alloc;

//! \todo Probably won't only hold epochs.
static struct yarn_pstore* g_epoch_store;

// Heads for the addr_info linked list of each epoch.
static struct addr_info* g_info_list[YARN_EPOCH_MAX];

// Quick element access to bypass the hash table.
static struct addr_info** g_info_index;
static size_t g_info_index_size;



// Prototypes

static inline size_t addr_info_size ();

static inline yarn_word_t get_epoch (yarn_word_t pool_id);
static inline yarn_word_t index_to_epoch_after (yarn_word_t base_epoch, 
						yarn_word_t index);
static inline yarn_word_t index_to_epoch_before (yarn_word_t base_epoch, 
						 yarn_word_t index);


static inline void release_addr_info (struct addr_info* info);
static inline struct addr_info* acquire_map_addr_info (yarn_word_t pool_id, 
						       const void* addr);
static inline struct addr_info* acquire_index_addr_info (yarn_word_t pool_id,
							 yarn_word_t index_id,
							 const void* addr);

static inline void info_list_push (yarn_word_t epoch, struct addr_info* info);
static inline struct addr_info* info_list_pop (yarn_word_t epoch);

static inline void dep_violation_check (yarn_word_t epoch, yarn_word_t read_flags);

static inline void store_to_wbuf (struct addr_info* info, yarn_word_t epoch, 
				  const void* src, void* dest, size_t size);
static inline void load_from_wbuf (struct addr_info* info, yarn_word_t epoch, 
				   const void* src, void* dest, size_t size); 

static inline void alignment_check (const void* addr);

// Use inline to prevent the compiler from whining.
static inline void dump_info(struct addr_info* info);
static inline void dump_flags(yarn_word_t f);





static bool addr_info_construct(void* data) { 
  struct addr_info* info = (struct addr_info*) data;

  int ret = pthread_mutex_init(&info->lock, NULL);
  if (ret) goto mutex_error;

  info->info_list = (struct addr_info**) (info->write_buffer + YARN_EPOCH_MAX);
  for (size_t i = 0; i < YARN_EPOCH_MAX; ++i) {
    info->info_list[i] = NULL;
  }

  info->last_commit = -1;
  info->write_flags = 0;
  info->read_flags = 0;

  return true;

  // pthread_mutex_destroy(&info->lock);
 mutex_error:
  perror(__FUNCTION__);
  return false;
}

static void addr_info_destruct(void* data) {
  struct addr_info* info = (struct addr_info*) data;
  pthread_mutex_destroy(&info->lock);
}

static void map_item_destruct (void* data) {
  addr_info_destruct(data);
  free(data);
}




bool yarn_dep_global_init (size_t ws_size, yarn_word_t index_size) {
  g_dependency_map = yarn_map_init(ws_size);
  if (!g_dependency_map) goto map_error;

  g_addr_info_alloc = yarn_pmem_init(addr_info_size(), 
				     addr_info_construct, 
				     addr_info_destruct);
  if (!g_addr_info_alloc) goto allocator_error;

  g_epoch_store = yarn_pstore_init();
  if (!g_epoch_store) goto epoch_store_error;
  for (size_t i = 0; i < YARN_EPOCH_MAX; ++i) {
    g_info_list[i] = NULL;
  }

  g_info_index_size = index_size;
  g_info_index = (struct addr_info**) malloc(index_size * sizeof(struct addr_info*));
  if (!g_info_index) goto index_alloc_error;
  for (yarn_word_t i = 0; i < index_size; ++i) {
    g_info_index[i] = NULL;
  }


  return true;
  
  free(g_info_index);
 index_alloc_error:
  yarn_pstore_destroy(g_epoch_store);
 epoch_store_error:
  yarn_pmem_destroy(g_addr_info_alloc);
 allocator_error:
  yarn_map_destroy(g_dependency_map, map_item_destruct);
 map_error:
  perror(__FUNCTION__);
  return false;
}

void yarn_dep_global_destroy (void) {
  yarn_map_destroy(g_dependency_map, map_item_destruct);
  yarn_pmem_destroy(g_addr_info_alloc);

  for (yarn_word_t pool_id = 0; pool_id < yarn_tpool_size(); ++pool_id) {
    yarn_word_t* p_epoch = (yarn_word_t*) yarn_pstore_load(g_epoch_store, pool_id);
    if (p_epoch != NULL) {
      free(p_epoch);
    }
  }

  yarn_pstore_destroy(g_epoch_store);
  free(g_info_index);
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



bool yarn_dep_store (yarn_word_t pool_id, const void* src, void* dest) {
  alignment_check(src);

  yarn_word_t read_flags;
  const yarn_word_t epoch = get_epoch(pool_id);

  // Buffer the write.
  {
    struct addr_info* info = acquire_map_addr_info(pool_id, dest);
    if (!info) goto acquire_error;

    read_flags = info->read_flags;
    store_to_wbuf(info, epoch, src, dest, sizeof(yarn_word_t));

    release_addr_info(info);
  }

  dep_violation_check (epoch, read_flags);

  return true;

 acquire_error:
  perror(__FUNCTION__);
  return false;
}


bool yarn_dep_store_fast (yarn_word_t pool_id, 
			  yarn_word_t index_id, 
			  const void* src, 
			  void* dest)
{
  alignment_check(src);

  yarn_word_t read_flags;
  const yarn_word_t epoch = get_epoch(pool_id);

  // Buffer the write.
  {
    struct addr_info* info = acquire_index_addr_info(pool_id, index_id, dest);
    if (!info) goto acquire_error;

    read_flags = info->read_flags;
    store_to_wbuf(info, epoch, src, dest, sizeof(yarn_word_t));

    release_addr_info(info);
  }

  dep_violation_check (epoch, read_flags);

  return true;

 acquire_error:
  perror(__FUNCTION__);
  return false;

}




bool yarn_dep_load (yarn_word_t pool_id, const void* src, void* dest) {
  alignment_check(src);

  const yarn_word_t epoch = get_epoch(pool_id);

  {
    struct addr_info* info = acquire_map_addr_info(pool_id, src);
    if (!info) goto acquire_error;

    load_from_wbuf(info, epoch, src, dest, sizeof(yarn_word_t));

    release_addr_info(info);
  }
 
  return true;
  
 acquire_error:
  perror(__FUNCTION__);
  return false;
}

bool yarn_dep_load_fast (yarn_word_t pool_id, 
			 yarn_word_t index_id, 
			 const void* src, 
			 void* dest) 
{
  alignment_check(src);

  const yarn_word_t epoch = get_epoch(pool_id);

  {
    struct addr_info* info = acquire_index_addr_info(pool_id, index_id, src);
    if (!info) goto acquire_error;

    load_from_wbuf(info, epoch, src, dest, sizeof(yarn_word_t));

    release_addr_info(info);
  }
 
  return true;
  
 acquire_error:
  perror(__FUNCTION__);
  return false;
}





void yarn_dep_commit (yarn_word_t epoch) {
  const yarn_word_t epoch_index = YARN_BIT_INDEX(epoch);
  const yarn_word_t epoch_mask = YARN_BIT_MASK(epoch);

  struct addr_info* info;
  while ((info = info_list_pop(epoch)) != NULL) {
    YARN_CHECK_RET0(pthread_mutex_lock(&info->lock));

    yarn_word_t flag_mask = yarn_bit_mask_range(yarn_epoch_first(), epoch+1);
    info->read_flags &= ~flag_mask;

    if (info->write_flags & epoch_mask) {
      info->write_flags &= ~flag_mask;

    // Write the value to memory only if no newer value was already written.
      if (yarn_timestamp_comp(epoch, info->last_commit) > 0) {

	yarn_word_t rb_mask = ~yarn_epoch_rollback_flags();
	yarn_word_t val =  info->write_buffer[epoch_index];
	printf("[%3zu] WRITTING -> {"YARN_SHEX"}=%zu"
	       "\t\t\t\t\t\t\t\trb_mask="YARN_SHEX", wf="YARN_SHEX", rf="YARN_SHEX"\n",
	       epoch, YARN_AHEX((uintptr_t)info->addr), val, YARN_AHEX(rb_mask),
	       YARN_AHEX(info->write_flags), YARN_AHEX(info->read_flags));
	memcpy(info->addr, &info->write_buffer[epoch_index], sizeof(yarn_word_t));
	info->last_commit = epoch;
      }
    }
    
    YARN_CHECK_RET0(pthread_mutex_unlock(&info->lock));
  }
}


void yarn_dep_rollback (yarn_word_t epoch) {
  struct addr_info* info;
  while ((info = info_list_pop(epoch)) != NULL) {    
    YARN_CHECK_RET0(pthread_mutex_lock(&info->lock));
    
    info->write_flags = YARN_BIT_CLEAR(info->write_flags, epoch);
    info->read_flags = YARN_BIT_CLEAR(info->read_flags, epoch);    

    yarn_word_t rb_mask = ~yarn_epoch_rollback_flags();
    printf("[%3zu] ROLLBACK -> {"YARN_SHEX"}"
	   "\t\t\t\t\t\t\t\trb_mask="YARN_SHEX", wf="YARN_SHEX", rf="YARN_SHEX"\n",
	   epoch, YARN_AHEX((uintptr_t) info->addr), YARN_AHEX(rb_mask),
	   YARN_AHEX(info->write_flags), YARN_AHEX(info->read_flags));

    YARN_CHECK_RET0(pthread_mutex_unlock(&info->lock));
  }  
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


static inline struct addr_info* acquire_index_addr_info (yarn_word_t pool_id,
							 yarn_word_t index_id,
							 const void* addr) 
{
  assert(index_id < g_info_index_size);

  struct addr_info* info;

  if (g_info_index[index_id] == NULL) {
    info = acquire_map_addr_info(pool_id, addr);
    if (!info) goto acquire_error;

    g_info_index[index_id] = info;
  }
  else {
    info = g_info_index[index_id];
    YARN_CHECK_RET0(pthread_mutex_lock(&info->lock));
  }

  return info;

  // release_addr_info(info);
 acquire_error:
  perror(__FUNCTION__);
  return NULL;
}


static inline struct addr_info* acquire_map_addr_info (yarn_word_t pool_id, 
						       const void* addr) 
{
  struct addr_info* tmp_info = yarn_pmem_alloc(g_addr_info_alloc, pool_id);
  if (!tmp_info) goto alloc_error;

  const yarn_word_t epoch = get_epoch(pool_id);

  YARN_CHECK_RET0(pthread_mutex_lock(&tmp_info->lock));
  
  struct addr_info* info = (struct addr_info*) 
      yarn_map_probe(g_dependency_map, (uintptr_t)addr, tmp_info);
  info->addr = (void*) addr; //! \todo It's either this or remove all const qualifiers.
  
  if (info != tmp_info) {
    YARN_CHECK_RET0(pthread_mutex_unlock(&tmp_info->lock));
    yarn_pmem_free(g_addr_info_alloc, pool_id, tmp_info);
    tmp_info = NULL;
    YARN_CHECK_RET0(pthread_mutex_lock(&info->lock));

    const yarn_word_t mask = YARN_BIT_MASK(epoch);
    if ((info->read_flags & mask) == 0 && (info->write_flags & mask) == 0) {
      info_list_push(epoch, info);
    }
  }
  else {
    info_list_push(epoch,info);
  }

  return info;

 alloc_error:
  perror(__FUNCTION__);
  return NULL;
}


static inline void release_addr_info (struct addr_info* info) {
  YARN_CHECK_RET0(pthread_mutex_unlock(&info->lock));  
}




static inline void info_list_push (yarn_word_t epoch, struct addr_info* info) {
  const yarn_word_t index = YARN_BIT_INDEX(epoch);

  info->info_list[index] = g_info_list[index];
  g_info_list[index] = info;
}

static inline struct addr_info* info_list_pop (yarn_word_t epoch) {
  const yarn_word_t index = YARN_BIT_INDEX(epoch);
  struct addr_info* head = g_info_list[index];

  if (head == NULL) {
    return NULL;
  }

  g_info_list[index] = head->info_list[index];
  head->info_list[index] = NULL;

  return head;
}




static inline void store_to_wbuf (struct addr_info* info, 
				  yarn_word_t epoch, 
				  const void* src, 
				  void* dest,
				  size_t size) 
{
  (void) dest;
  const yarn_word_t epoch_index = YARN_BIT_INDEX(epoch);

  info->write_flags = YARN_BIT_SET(info->write_flags, epoch);
  memcpy(&info->write_buffer[epoch_index], src, size);

  yarn_word_t rb_mask = ~yarn_epoch_rollback_flags();
  printf("[%3zu] STORE    -> {"YARN_SHEX"}=%zu"
	 "\t\t\t\t\t\t\t\trb_mask="YARN_SHEX", wf="YARN_SHEX", rf="YARN_SHEX"\n",
	 epoch, YARN_AHEX((uintptr_t)info->addr), 
	 info->write_buffer[epoch_index], YARN_AHEX(rb_mask),
	 YARN_AHEX(info->write_flags), YARN_AHEX(info->read_flags));
}

static inline void load_from_wbuf (struct addr_info* info, 
				   yarn_word_t epoch, 
				   const void* src, 
				   void* dest, 
				   size_t size)
{
  info->read_flags = YARN_BIT_SET(info->read_flags, epoch);
  yarn_word_t write_flags = info->write_flags;

  // Could realease the lock here but requires some extra checks.

  const yarn_word_t rollback_mask = ~yarn_epoch_rollback_flags();
  write_flags &= rollback_mask;

  const yarn_word_t first_epoch = yarn_epoch_first();
  const yarn_word_t first_index = YARN_BIT_INDEX(first_epoch);
  const yarn_word_t last_epoch = epoch+1;
  const yarn_word_t last_index = YARN_BIT_INDEX(epoch+1);


  yarn_word_t flags;
  yarn_word_t mask;

  if (first_index <= last_index) {
    // Must use the epochs here (the index might be equal but the epochs might not).
    mask = yarn_bit_mask_range(first_epoch, last_epoch);
    flags = write_flags & mask;
  }
  else {
    mask = yarn_bit_mask_range(0, last_index);
    flags = write_flags & mask;
    if (flags == 0) {
      yarn_word_t second_mask = yarn_bit_mask_range(first_index, YARN_EPOCH_MAX);
      mask |= second_mask;
      flags = write_flags & second_mask;
    }
  }

  // Read the earliest write in the buffer.
  if (flags) {
    const yarn_word_t read_index = yarn_bit_log2(flags);

    { // DEBUG
      const yarn_word_t read_epoch = index_to_epoch_before(epoch, read_index);
      printf("[%3zu] LOAD     -> {"YARN_SHEX"}=%zu - BUF[%3zu]"
	     "\t\tfirst_e=%zu, (%zu, %zu), mask="YARN_SHEX", rb_mask="YARN_SHEX
	     ", wf="YARN_SHEX", rf="YARN_SHEX"\n",
	     epoch, YARN_AHEX((uintptr_t)info->addr),
	     info->write_buffer[read_index], read_epoch, 
	     first_epoch, first_index, last_index,
	     YARN_AHEX(mask), YARN_AHEX(rollback_mask),
	     YARN_AHEX(info->write_flags), YARN_AHEX(info->read_flags));
    }

    memcpy(dest, &info->write_buffer[read_index], size);
  }

  // No value in the buffer, go to memory.
  else {
    { // DEBUG
      yarn_word_t t_val;
      memcpy(&t_val, src, size);
      printf("[%3zu] LOAD     -> {"YARN_SHEX"}=%zu - MEM"
	     "\t\tfirst_e=%zu, (%zu, %zu), mask="YARN_SHEX", rb_mask="YARN_SHEX
	     ", wf="YARN_SHEX", rf="YARN_SHEX"\n",
	     epoch, YARN_AHEX((uintptr_t)info->addr), t_val, 
	     first_epoch, first_index, last_index,
	     YARN_AHEX(mask), YARN_AHEX(rollback_mask), 
	     YARN_AHEX(info->write_flags), YARN_AHEX(info->read_flags));
    }
    
    memcpy(dest, src, size);
  }
}



static inline void dep_violation_check (yarn_word_t epoch, yarn_word_t read_flags) {

  const yarn_word_t first_epoch = epoch+1;
  const yarn_word_t last_epoch = yarn_epoch_last();
  if (first_epoch >= last_epoch) {
    return;
  }

  const yarn_word_t rollback_mask = ~yarn_epoch_rollback_flags();  
  read_flags &= rollback_mask;

  const yarn_word_t first_index = YARN_BIT_INDEX(first_epoch);
  const yarn_word_t last_index = YARN_BIT_INDEX(last_epoch);

  yarn_word_t flags;
  if (first_index < last_index) {
    // Must use the epochs here (the index might be equal but the epochs might not).
    flags = read_flags & yarn_bit_mask_range(first_epoch, last_epoch);
  }
  else {
    flags = read_flags & yarn_bit_mask_range(first_index, YARN_EPOCH_MAX);
    if (flags == 0) {
      flags = read_flags & yarn_bit_mask_range(0, last_index);
    }
  }

  if (flags == 0) {
    return;
  }    

  yarn_word_t rollback_index = yarn_bit_trailing_zeros(flags);
  yarn_word_t rollback_epoch = index_to_epoch_after(epoch, rollback_index);
  yarn_epoch_do_rollback(rollback_epoch);

}


static inline void alignment_check (const void* addr) {
  assert(sizeof(yarn_word_t) == 4 ? 
	 ((uintptr_t)addr & ~3) == (uintptr_t)addr : 
	 ((uintptr_t)addr & ~7) == (uintptr_t)addr);
}


static inline void dump_info(struct addr_info* info) {
  printf("INFO["YARN_SHEX"] -> commit=%zu, wf="YARN_SHEX", rf="YARN_SHEX"\n", 
	 YARN_AHEX((uintptr_t)info->addr), info->last_commit,
	 YARN_AHEX(info->write_flags), YARN_AHEX(info->read_flags));

  
  /*
  printf("INFO["YARN_SHEX"] -> commit=%zu", 
	 YARN_AHEX((uintptr_t)info->addr), info->last_commit);

  printf(", writef=");
  dump_flags(info->write_flags);
  printf(", readf=");
  dump_flags(info->read_flags);
  */
  /*
  printf (", wbuf={");
  for (yarn_word_t i = 0; i < YARN_EPOCH_MAX; ++i) {
    if ((info->write_flags & YARN_BIT_MASK(i)) != 0) {
      printf("%zu -> "YARN_SHEX", ", i, YARN_AHEX(info->write_buffer[i]));
    }
  }
  printf("}\n");
  */
}

static inline void dump_flags(yarn_word_t f) {
  printf("{");
  
  yarn_word_t b;
  while (f != 0) {

    b = yarn_bit_log2(f);
    f = YARN_BIT_CLEAR(f, b);

    printf("%zu", b);
    if (f != 0) printf(",");
  }
  printf("}");
}

