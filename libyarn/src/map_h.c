/*!
\author Rémi Attab
\license FreeBSD (see LICENSE file)

\brief Open addressing hash table implementation for the dictionnary.

Implements linear probing and 
 */

#include "map.h"
#include "atomic.h"
#include "alloc.h"

#include <math.h>
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>


//! Default capacity of the hash table.
//! \todo Should be f(num_core) to avoid having 2 legitimate resizes in a row.
#define YARN_MAP_DEFAULT_CAPACITY 64

//! Treshold used to determined when the hash table must be resized.
#define YARN_MAP_LOAD_FACTOR 0.66f

//! Treshold when the resize helpers will stop trying to transfer items.
#define YARN_MAP_HELPER_TRESHOLD 8


enum resize_state {
  state_nothing = 0,
  state_preparing,
  state_resizing,
  state_waiting
};


struct map_node {
  yarn_atomic_ptr addr;
  yarn_atomic_ptr value;
};

struct yarn_map {
  struct map_node* table;
  yarn_atomic_var size;
  size_t capacity;

  struct map_node* new_table;
  size_t new_capacity;
  
  yarn_atomic_var resize_pos;
  yarn_atomic_var user_count;
  yarn_atomic_var helper_count;
  yarn_atomic_var resize_state;
};


static inline size_t hash(uintptr_t h, size_t capacity);
static void init_table (struct map_node** table, size_t capacity);
static void resize_master (struct yarn_map* m);
static void resize_helper (struct yarn_map* m);



static void init_table (struct map_node** table, size_t capacity) {
  size_t table_size = capacity * sizeof(struct map_node);

  *table = yarn_malloc(table_size);
  for(size_t i = 0; i < capacity; ++i) {
    yarn_writep(&(*table)[i].addr, NULL);
    yarn_writep(&(*table)[i].value, NULL);
  }
}


struct yarn_map* yarn_map_init (size_t capacity) {
  struct yarn_map* map = yarn_malloc(sizeof(struct yarn_map));
  
  map->capacity = YARN_MAP_DEFAULT_CAPACITY;
  size_t load_factor = (float)capacity / YARN_MAP_LOAD_FACTOR;
  while(map->capacity < load_factor)
    map->capacity <<= 1;

  yarn_writev(&map->size, 0);
  init_table(&map->table, map->capacity);

  yarn_writev(&map->user_count, 0);
  yarn_writev(&map->helper_count, 0);
  yarn_writev(&map->resize_state, 0);

  return map;
}


//! \todo Make sure we get something to clean up the values. Probably a fct ptr.
void yarn_map_free (struct yarn_map* m) {
  yarn_free(m->table);
  yarn_free(m);
}


void* yarn_map_probe (struct yarn_map* m, uintptr_t addr, void* value) {
  assert (value == NULL);
  
  yarn_incv(&m->user_count);
  if (yarn_readv(&m->resize_state) != state_nothing) {
    yarn_decv(&m->user_count);
    resize_helper(m);
    yarn_incv(&m->user_count);
  }
  
  const size_t h = hash(addr, m->capacity);
  void* return_val = NULL;
  size_t i = h;
  size_t n = 0;
  size_t new_size = yarn_readv(&m->size);

  // linear probe
  while (n < m->capacity) {
    yarn_atomic_ptr* probe_addr = &m->table[i].addr;
    yarn_atomic_ptr* probe_val = &m->table[i].value;
    void* read_addr = yarn_readp(probe_addr);

    // Did we find our value?
    if ((uintptr_t)read_addr == addr) {
      // make sure the value is available (it's not added atomically with addr).
      yarn_spinp_neq(probe_val, NULL);
      return_val = yarn_readp(probe_val);
      break;
    }

    // Do we have an empty bucket to add the value?
    else if (read_addr == NULL) {
      if (yarn_casp(probe_addr, NULL, (void*) addr) != NULL)
	continue; // retry the same bucket if we weren't able to add.
      yarn_writep(probe_val, value);
      return_val = value;
      new_size = yarn_incv(&m->size);
      break;
    }

    i = (i+1) % m->capacity;
    n++;
  }

  float load_factor = (float)new_size / (float)m->capacity;
  if (load_factor > YARN_MAP_LOAD_FACTOR) {
    resize_master(m);
  }
  // User count is handled specially by resize_master to avoid the double resize problem.
  else {
    yarn_decv(&m->user_count);
  }
  
  // If we were not able to add the item, then try again.
  return return_val != NULL ? return_val : yarn_map_probe(m, addr, value);
}


/*!
Transfers an item at position \c pos in the current table to the new table.
Note that this can be called concurrently by the master and all its helper for a single
location. Also, an item can be transfered at most once so calling the function twice
for the operation will result in one of those calls in being a no-op.
 */
static void transfer_item (struct yarn_map* m, size_t pos) {

  void* addr = yarn_readp(&m->table[pos].addr);
  if (addr == NULL || yarn_casp(&m->table[pos].addr, addr, NULL) != addr) {
    return;
  }

  const size_t h = hash((uintptr_t)addr, m->new_capacity);
  size_t i = h;
  size_t n = 0;
  
  while (n < m->new_capacity) {
    if (yarn_casp_fast(&m->new_table[pos].addr, NULL, addr) == NULL) {
      void* probe_value = yarn_readp(&m->table[pos].value);
      yarn_writep(&m->new_table[pos].value, probe_value);
      break;
    }
    
    i = (i+1) % m->new_capacity;
    n++;
  }
}


/*!
Main resizer that linearly transfers all the items.
There can be at most one master resizer working at any given time. Any other thread that
tries to resize will become a helper. This function also handles the decrement of the
user count value so the caller should not decrement it if calling this function. This is
done to ensure that we can resolve any master selection conflict before any resizing
is done. Otherwise we risk of having two conflicting resizes being started one after the
other.

To avoid any concurrency issues, no threads should be allowed to read the table while
\code resize_state != state_nothing \endcode. This function will then wait for \code
user_count == 1 \endcode (the last user being the master) before manipulating the original
table. Once the transfer is completed, the function will wait for \code helper_count == 0
\endcode before swapping the old table with the new table and cleaning up.
 */
static void resize_master (struct yarn_map* m) {

  // Get master ownership. If we fail, then become a helper.
  if (yarn_casv_fast(&m->resize_state, state_nothing, state_preparing) != state_nothing) {
    // The master keeps its user_count to avoid ending up with 2 master
    // If we fallback to a helper then we give up our user_count ownership.
    yarn_decv(&m->user_count);
    resize_helper(m);
    return;
  }

  // Init the new table.
  m->new_capacity = m->capacity *2;
  init_table(&m->new_table, m->new_capacity);
  
  // let regular users drain out.
  yarn_spinv_eq(&m->user_count, 1);

  // Start the transfer
  yarn_writev(&m->resize_pos, 0);
  yarn_writev_barrier(&m->resize_state, state_resizing);
  for (size_t pos = 0; pos < m->capacity; ++pos, yarn_incv(&m->resize_pos)) {
    transfer_item(m, pos);
  }

  // Wait for the helpers to finish.
  yarn_writev_barrier(&m->resize_state, state_waiting);
  yarn_spinv_eq(&m->helper_count, 0);

  // swap the tables and clean up.
  yarn_free(m->table);
  m->table = m->new_table;
  m->new_table = NULL;
  m->capacity = m->new_capacity;
  m->new_capacity = 0;

  // we're done.
  yarn_writev_barrier(&m->resize_state, state_nothing);
  yarn_decv(&m->user_count);
}


/*!
Helper resizer function that randomly picks an item to transfer.
This function \b MUST \b NOT be called with an incremented \c user_count. This function
follows the concurrency specifications of the \c resize_master function.
 */ 
static void resize_helper (struct yarn_map* m) {
  yarn_incv(&m->helper_count);

  if (yarn_readv(&m->resize_state) == state_nothing) {
    yarn_decv(&m->helper_count);
    return;
  }

  // wait for preperations to be over.
  yarn_spinv_neq(&m->resize_state, state_preparing);

  // start transfering items.
  while (yarn_readv(&m->resize_state) == state_resizing) {
    size_t min_pos = yarn_readv(&m->resize_pos) + YARN_MAP_HELPER_TRESHOLD;
    size_t range = m->capacity - min_pos;

    if (range > YARN_MAP_HELPER_TRESHOLD) {
      size_t pos = ((float)rand() / RAND_MAX) * range + min_pos;
      transfer_item(m, pos);
    }
    // Not enough items left to make it worth it to continue.
    else 
      break;
  }

  // Wait for master to complete transfering items.
  yarn_decv(&m->helper_count);
  yarn_spinv_neq(&m->resize_state, state_resizing);

  // wait for master to finish cleaning up.
  yarn_spinv_neq(&m->resize_state, state_waiting);
}



/*!
32 or 64 bit hashing function.
The function is the fmix functions from MurmurHash3 written by Austin Appleby which evenly 
mixes every bits of the of the given variable.

Original can be found here: http://code.google.com/p/smhasher/
 */
static inline size_t hash(uintptr_t h, size_t capacity) {

#if (UINTPTR_MAX == UINT32_MAX)

  h ^= h >> 16;
  h *= 0x85ebca6b;
  h ^= h >> 13;
  h *= 0xc2b2ae35;
  h ^= h >> 16;

#elif (UINTPTR_MAX == UINT64_MAX)

  h ^= h >> 33;
  h *= 0xff51afd7ed558ccdLLU;
  h ^= h >> 33;
  h *= 0xc4ceb9fe1a85ec53LLU;
  h ^= h >> 33;

#else
#error "map_h.c: Hash function only works on 16 or 32 bit addresses."
#endif

  return (size_t) (h % capacity);
}


