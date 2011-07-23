/*!
\author Rémi Attab
\license FreeBSD (see the LICENSE file)
 */

#ifndef YARN_YARN_H_
#define YARN_YARN_H_


#include "yarn/dependency.h"

enum yarn_ret {
  yarn_ret_continue = 0,
  yarn_ret_break = 1,
  yarn_ret_error = 2
};

typedef enum yarn_ret (*yarn_executor_t) (const yarn_word_t pool_id, void* data);

bool yarn_init (void);
void yarn_destroy (void);

#define YARN_ALL_THREADS ((yarn_word_t)0) // See YARN_TPOOL_ALL_THREADS

bool yarn_exec_simple (yarn_executor_t executor, 
		       void* data, 
		       yarn_word_t thread_count,
		       yarn_word_t ws_size, 
		       yarn_word_t index_size);

yarn_word_t yarn_thread_count();


#endif // YARN_YARN_H_
