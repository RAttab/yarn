/*!
\author Rémi Attab
\license FreeBSD (see LICENSE file)

\brief Open addressing hash table implementation for the dictionnary.

Implements linear probing and 
 */

#include "map.h"
#include "atomic.h"

#include <stddef.h>
#include <stdbool.h>
#include <string.h>


//! Default capacity of the hash table.
#define YARN_MAP_DEFAULT_CAPACITY 64

//! Treshold used to determined when the hash table must be resized.
#define YARN_MAP_LOAD_FACTOR 0.66


/*!
32 or 64 bit hashing function.
The function is the fmix function from MurmurHash3 which evenly mixes the bits of the 
of the given variable.
 */
static inline size_t hash(uintptr_t h, size_t capacity);

static void resize_master (struct yarn_map* m);
static void resize_helper (struct yarn_map* m);


enum resize_state {
  state_nothing = 0,
  state_preparing,
  state_resizing,
  state_waiting;
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
  yarn_atomic_var resize_count;
  yarn_atomic_var resize_state;
};


struct yarn_map* yarn_map_init (size_t capacity) {
  struct yarn_map* map = yarn_malloc(sizeof(struct yarn_map));
  
  map->capacity = 1;
  if(capacity <= 0)
    capcity = YARN_MAP_DEFAULT_CAPACITY;

  while(map->capacity < capacity*4/3)
    map->capacity <<= 1;

  yarn_writev(&map->size, 0);
  size_t table_size = map->capacity * sizeof(struct map_node);

  map->table = yarn_malloc(table_size);
  for(size_t i = 0; i < map->capacity; ++i) {
    yarn_writep(&map->table[i].addr, NULL);
    yarn_writep(&map->table[i].value,NULL;
  }

  yarn_writev(&user_count, 0);
  yarn_writev(&resize_count, 0);
  yarn_writev(&resize_state, 0);
}


//! \todo Make sure we get something to clean up the values. Probably a fct ptr.
void yarn_map_free (struct yarn_map* m) {
  if (m->new_table != NULL)
    yarn_free(m->new_table);
  yarn_free(m->table);
  yarn_free(m);
}



void* yarn_map_probe (struct yarn_map* m, uintptr_t addr, void* value) {
  
  yarn_incv(&m->user_count);
  if (m->resize_state == state_nothing) {
    yarn_decv(&m->user_count);
    resize_helper(m);
    yarn_incv(&m->user_count);
  }
  
  const size_t h = hash(addr, m->capacity);
  void* return_val = NULL;
  size_t i = h;
  size_t n = 0;

  // linear probe
  while (n < m->capacity) {
    struct yarn_atomp_t* probe_addr = &m->table[i].addr;
    struct yarn_atomp_t* probe_val = &m->table[i].value;
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
      if (yarn_casp(probe_addr, NULL, (voir*) addr) != NULL)
	continue; // retry the same bucket if weren't able to add.
      yarn_writep(probe_val, value);
      yarn_incv(&m->size);
      return_val = value;
      break;
    }

    i = (i+1) % capacity;
    n++;
  }

  yarn_decv(&m->size);

  float load_factor = (float)yarn_readv(&m->size) / (float)(m->capacity);
  if (load_factor > YARN_MAP_LOAD_FACTOR)
    resize_master(m);
  
  return return_val;
}

static void resize_master (struct yarn_map* m) {
  //! \todo
}

 
static void resize_helper (struct yarn_map* m) {
  //! \todo
}




static inline size_t hash(uintptr_t h, size_t capacity) {

#ifdef UINTPTR_MAX == UINT32_MAX

  h ^= h >> 16;
  h *= 0x85ebca6b;
  h ^= h >> 13;
  h *= 0xc2b2ae35;
  h ^= h >> 16;

#elif UINTPTR_MAX == UINT64_MAX

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


