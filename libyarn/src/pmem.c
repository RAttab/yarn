/*!
\author Rémi Attab
\license FreeBSD (see the LICENSE file).

Pool allocation.
 */


#include "pmem.h"

#include "pstore.h"

#include <stdio.h>
#include <stdlib.h>


struct yarn_pmem {
  size_t size;
  yarn_pmem_construct construct_fun;
  yarn_pmem_destruct destruct_fun;
  struct yarn_pstore* cache;
};



struct yarn_pmem* 
yarn_pmem_init(size_t size, yarn_pmem_construct cons_fun, yarn_pmem_destruct des_fun) {


  struct yarn_pmem* m = (struct yarn_pmem*) malloc(sizeof(struct yarn_pmem));
  if (!m) goto alloc_error;

  m->cache = yarn_pstore_init();
  if (!m->cache) goto pstore_error;

  m->size = size;
  m->construct_fun = cons_fun;
  m->destruct_fun = des_fun;

  return m;

 pstore_error:
  free(m);
 alloc_error:
  perror(__FUNCTION__);
  return NULL;
}


void yarn_pmem_destroy(struct yarn_pmem* m) {
  for (yarn_word_t pool_id = 0; pool_id < yarn_pstore_size(m->cache); ++pool_id) {
    void* data = yarn_pstore_load(m->cache, pool_id);
    if (data == NULL) {
      continue;
    }

    if (m->destruct_fun) {
      (*m->destruct_fun)(data);
    }

    free(data);
  }
  
  if(!m->cache) {
    yarn_pstore_destroy(m->cache);
  }

  free(m);
}


void* yarn_pmem_alloc(struct yarn_pmem* m, yarn_word_t pool_id) {

  void* data = yarn_pstore_load(m->cache, pool_id);

  if (data == NULL) {    
    data = malloc(m->size);
    if (!data) goto alloc_error;

    if (m->construct_fun && !(*m->construct_fun)(data)) {
      free(data);
      return NULL;
    }
  }
  else {
    yarn_pstore_store(m->cache, pool_id, NULL);
  }

  return data;

 alloc_error:
  perror(__FUNCTION__);
  return NULL;
}


void yarn_pmem_free(struct yarn_pmem* m, yarn_word_t pool_id, void* data) {
  void* cached_data = yarn_pstore_load(m->cache, pool_id);
  if (cached_data == NULL) {
    yarn_pstore_store(m->cache, pool_id, data);
  }
  else {
    if (m->destruct_fun) {
      (*m->destruct_fun)(data);
    }
    free(data);
  }
}
