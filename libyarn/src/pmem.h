/*!
\author Rémi Attab
\license FreeBSD (see the LICENSE file).

Pool allocation.
 */

#ifndef YARN_PMEM_H_
#define YARN_PMEM_H_


#include "yarn/types.h"
#include "tpool.h"
#include "pstore.h"


typedef bool (*yarn_pmem_construct) (void*);
typedef void (*yarn_pmem_destruct) (void*);

struct yarn_pmem;

struct yarn_pmem* 
yarn_pmem_init(size_t size, yarn_pmem_construct cons_fun, yarn_pmem_destruct des_fun);
void yarn_pmem_destroy(struct yarn_pmem* m);

void* yarn_pmem_alloc(struct yarn_pmem* m, yarn_word_t pool_id);
void yarn_pmem_free(struct yarn_pmem* m, yarn_word_t pool_id, void* data);
void yarn_pmem_free_seq(struct yarn_pmem* m, void* data);



#endif // YARN_PMEM_H_
