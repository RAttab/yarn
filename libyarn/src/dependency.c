/*!
\author Rémi Attab
\license FreeBSD (see the LICENSE file)

 */


#include <dependency.h>

#include "atomic.h"
#include "map.h"
#include "timestamp.h"
#include "alloc.h"
#include <threads.h>

#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>


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

// Since we'll have to "pointlessly" allocate this quite often we create a small cache
//  to avoid excessive mallocs.
static int g_addr_dep_cache_size;
static struct addr_dep** g_addr_dep_cache;



static inline struct addr_dep* alloc_addr_dep (yarn_tsize_t pool_id);
static inline void free_addr_dep (yarn_tsize_t pool_id, struct addr_dep* s);


// ---------------------------------------------------------------------------------------
// Placeholders.

static inline yarn_ts_sample_t get_epoch () {
  //! \todo Return whatever is stored in the thread local.
  return 0;
}

static inline void set_epoch (yarn_ts_sample_t epoch) {
  //! \todo Update the thread local.
  (void) epoch;
}


//
// ---------------------------------------------------------------------------------------

static inline size_t addr_dep_size() {
  return sizeof(struct addr_dep*) + sizeof(wbuf_t) * yarn_tpool_size();
}




bool yarn_dep_global_init (size_t ws_size) {
  g_dependency_map = yarn_map_init(ws_size);

  g_addr_dep_cache_size = yarn_tpool_size();
  g_addr_dep_cache = yarn_malloc (addr_dep_size() * g_addr_dep_cache_size);
  for (int i = 0; i < g_addr_dep_cache_size; ++i) {
    g_addr_dep_cache[i] = NULL;
  }

  return true;
}

void yarn_dep_global_cleanup () {
  yarn_map_free(g_dependency_map);
  
  for (int i = 0; i < g_addr_dep_cache_size; ++i) {
    if (g_addr_dep_cache[i] != NULL) {
      pthread_mutex_destroy(&g_addr_dep_cache[i]->lock);
      yarn_free(g_addr_dep_cache[i]);
    }
  }
  yarn_free(g_addr_dep_cache);
}


bool yarn_dep_thread_init ();
void yarn_dep_thread_cleanup ();


void yarn_dep_store (yarn_tsize_t pool_id, void* addr, size_t size) {
  assert(size <= sizeof(wbuf_t));

  struct addr_dep* tmp_dep = alloc_addr_dep(pool_id);
  pthread_mutex_lock(&tmp_dep->lock);
  
  struct addr_dep* dep = (struct addr_dep*) 
    yarn_map_probe(g_dependency_map, (uintptr_t)addr, tmp_dep);
  
  if (dep != tmp_dep) {
    pthread_mutex_unlock(&tmp_dep->lock);
    free_addr_dep(pool_id, tmp_dep);
    tmp_dep = NULL;
    pthread_mutex_lock(&dep->lock);
  }

  uint32_t read_flags = dep->read_flags;
  (void) read_flags;
  /*!

    \todo Finish the bit manip stuff.

  dep->write_flags |= 1 << ???;
  memcpy(dep->write_buffer[???], addr, size);
  */

  pthread_mutex_unlock(&dep->lock);


  //! \todo check the read flags and rollback if necessary.

}

uint_fast32_t yarn_dep_loadv (yarn_tsize_t pool_id, void* addr, size_t size) {
  (void) pool_id;
  (void) addr;
  (void) size;
  return 0; //! \todo
}

uintptr_t yarn_dep_loadp (yarn_tsize_t pool_id, void* addr) {
  (void) pool_id;
  (void) addr;
  return 0; //! \todo
}



//! \todo good candidate to make into a generic memory pool thingy.

static inline struct addr_dep* alloc_addr_dep (yarn_tsize_t pool_id) {
  struct addr_dep* ret = NULL;

  if (g_addr_dep_cache[pool_id] != NULL) {
    ret = g_addr_dep_cache[pool_id];
    g_addr_dep_cache[pool_id] = NULL;
    
  }
  else {
    ret = yarn_malloc(addr_dep_size());
    pthread_mutex_init(&ret->lock, NULL);
  }

  ret->write_flags = 0;
  ret->read_flags = 0;

  return ret;
}

static inline void free_addr_dep (yarn_tsize_t pool_id, struct addr_dep* s) {
  if (g_addr_dep_cache[pool_id] == NULL) {
    g_addr_dep_cache[pool_id] = s;
  }
  else if (g_addr_dep_cache[pool_id] != s) {
    pthread_mutex_destroy(&s->lock);
    yarn_free(s);
  }  
}


