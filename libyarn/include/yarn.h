/*!
\author Rémi Attab
\license FreeBSD (see the LICENSE file)
 */

#ifndef YARN_YARN_H_
#define YARN_YARN_H_


#include "yarn/dependency.h"

enum yarn_ret {
  yarn_ret_continue,
  yarn_ret_break,
  yarn_ret_error
};

typedef enum yarn_ret (*yarn_executor_t) (const yarn_word_t pool_id, void* data);

bool yarn_init (void);
void yarn_destroy (void);

bool yarn_exec_simple (yarn_executor_t executor, 
		       void* data, 
		       yarn_word_t ws_size, 
		       yarn_word_t index_size);


#endif // YARN_YARN_H_
