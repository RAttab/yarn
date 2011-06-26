/*!
\author Rémi Attab
\license FreeBSD (see the LICENSE file).


 */


#include "pstore.h"

#include <yarn/types.h>
#include "tpool.h"

#include <stdio.h>
#include <stdlib.h>


struct yarn_pstore* yarn_pstore_init(void) {
  const yarn_word_t pool_size = yarn_tpool_size();
  
  struct yarn_pstore* s = (struct yarn_pstore*)
    malloc(sizeof(struct yarn_pstore) + sizeof(void*) * pool_size);
  if (!s) goto alloc_error;

  s->size = pool_size;
  for (yarn_word_t pool_id = 0; pool_id != pool_size; ++pool_id) {
    s->data[pool_id] = NULL;
  }

  return s;

 alloc_error:
  perror(__FUNCTION__);
  return NULL;
}

void yarn_pstore_destroy(struct yarn_pstore* s) {
  free(s);
}

extern inline void* yarn_pstore_load(struct yarn_pstore* s, yarn_word_t pool_id);
extern inline void yarn_pstore_store(struct yarn_pstore* s, 
				     yarn_word_t pool_id, 
				     void* data);


