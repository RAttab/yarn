/*!
\author Rémi Attab
\license FreeBSD (see LICENSE file)

Interface for dependency checking.

Note that we don't yet support unaligned reads and writes. We don't because it can become
very difficult to track these types of things.
*/

#ifndef YARN_DEPENDENCY_H_
#define YARN_DEPENDENCY_H_


#include "tpool.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>


bool yarn_dep_thread_init (yarn_tsize_t pool_id);
void yarn_dep_thread_destroy (yarn_tsize_t pool_id);

bool yarn_dep_global_init (size_t ws_size);
void yarn_dep_global_destroy (void);

bool yarn_dep_store (yarn_tsize_t pool_id, void* addr, size_t size);

uint_fast32_t yarn_dep_loadv (yarn_tsize_t pool_id, void* addr, size_t size);
uintptr_t yarn_dep_loadp (yarn_tsize_t pool_id, void* addr);

void yarn_dep_commit (yarn_tsize_t pool_id);


#endif // YARN_DEPENDENCY_H_
