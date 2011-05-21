/*!
\author Rémi Attab
\license FreeBSD (see the LICENSE file)

 */


#include <dependency.h>

#include <types.h>
#include "tpool.h"
#include "atomic.h"
#include "map.h"
#include "timestamp.h"
#include "pmem.h"
#include "pstore.h"

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>


//! \todo gotta find something better then uint64_t
typedef uint64_t wbuf_t;

struct addr_dep {
  pthread_mutex_t lock;
  uint32_t read_flags;
  uint32_t write_flags;  
  wbuf_t write_buffer[];
};


// Contains all the dependency information for the speculative threads.
static struct yarn_map* g_dependency_map;

// Pool allocator for temp addr_dep values.
static struct yarn_pmem* g_addr_dep_alloc;

//! \todo Probably won't only hold epochs.
static struct yarn_pstore* g_epoch_store;



static inline size_t addr_dep_size() {
  return sizeof(struct addr_dep*) + sizeof(wbuf_t) * yarn_tpool_size();
}


static bool addr_dep_construct(void* data) { 
  struct addr_dep* d = (struct addr_dep*) data;

  int ret = pthread_mutex_init(&d->lock, NULL);
  if (!ret) goto mutex_error;

  return true;

 mutex_error:
  perror(__FUNCTION__);
  return false;
}

static void addr_dep_destruct(void* data) {
  struct addr_dep* d = (struct addr_dep*) data;
  pthread_mutex_destroy(&d->lock);
}




bool yarn_dep_global_init (size_t ws_size) {
  g_dependency_map = yarn_map_init(ws_size);
  if (!g_dependency_map) goto map_error;

  g_addr_dep_alloc = yarn_pmem_init(
      addr_dep_size(), addr_dep_construct, addr_dep_destruct);
  if (!g_addr_dep_alloc) goto allocator_error;

  g_epoch_store = yarn_pstore_init();
  if (!g_epoch_store) goto epoch_store_error;

  return true;

  // yarn_pstore_destroy(g_epoch_store);
 epoch_store_error:
  yarn_pmem_destroy(g_addr_dep_alloc);
 allocator_error:
  yarn_map_destroy(g_dependency_map);
 map_error:
  perror(__FUNCTION__);
  return false;
}

void yarn_dep_global_destroy (void) {
  yarn_map_destroy(g_dependency_map);
  yarn_pmem_destroy(g_addr_dep_alloc);

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
}




bool yarn_dep_store (yarn_word_t pool_id, void* addr, size_t size) {
  assert(size <= sizeof(wbuf_t));

  struct addr_dep* tmp_dep = yarn_pmem_alloc(g_addr_dep_alloc, pool_id);
  if (!tmp_dep) goto alloc_error;

  pthread_mutex_lock(&tmp_dep->lock);
  
  struct addr_dep* dep = (struct addr_dep*) 
    yarn_map_probe(g_dependency_map, (uintptr_t)addr, tmp_dep);
  
  if (dep != tmp_dep) {
    pthread_mutex_unlock(&tmp_dep->lock);
    yarn_pmem_free(g_addr_dep_alloc, pool_id, tmp_dep);
    tmp_dep = NULL;
    pthread_mutex_lock(&dep->lock);
  }

  uint32_t read_flags = dep->read_flags;
  (void) read_flags;
  /*!
  dep->write_flags |= 1 << ???;
  memcpy(dep->write_buffer[???], addr, size);
  */

  pthread_mutex_unlock(&dep->lock);


  //! \todo check the read flags and rollback if necessary.

  return true;

 alloc_error:
  perror(__FUNCTION__);
  return false;

}

uint_fast32_t yarn_dep_loadv (yarn_word_t pool_id, void* addr, size_t size) {
  (void) pool_id;
  (void) addr;
  (void) size;
  return 0; //! \todo
}

uintptr_t yarn_dep_loadp (yarn_word_t pool_id, void* addr) {
  (void) pool_id;
  (void) addr;
  return 0; //! \todo
}


