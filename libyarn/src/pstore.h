/*!
\author Rémi Attab
\license FreeBSD (see the LICENSE file).


Fast thread local storage.

 */


#ifndef YARN_PSTORE_H_
#define YARN_PSTORE_H_


#include "yarn/types.h"
#include "tpool.h"

#include <assert.h>


struct yarn_pstore {
  yarn_word_t size;
  void* data[];
};


struct yarn_pstore* yarn_pstore_init(void);
void yarn_pstore_destroy(struct yarn_pstore* s);


inline yarn_word_t yarn_pstore_size(struct yarn_pstore* s) {
  return s->size;
}

inline void* yarn_pstore_load(struct yarn_pstore* s, yarn_word_t pool_id) {
  assert(pool_id < s->size);
  return s->data[pool_id];
}

inline void yarn_pstore_store(struct yarn_pstore* s, yarn_word_t pool_id, void* data) {
  assert(pool_id < s->size);
  s->data[pool_id] = data;
}


#endif // YARN_PSTORE_H_
