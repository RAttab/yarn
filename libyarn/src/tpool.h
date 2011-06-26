/*!
\author Rémi Attab
\license FreeBSD (see LICENSE file)

Header for all thread related primitives.
*/


#ifndef YARN_TPOOL_H_
#define YARN_TPOOL_H_


#include <yarn/types.h>


/*!
\param uint_fast16_t The id of the thread in the pool. This number is easier to use then
the thread id provided by pthread because it's in the range 0 to yarn_tpool_size().

\param void* the task information passed to yarn_tpool_exec.

\return false if an error occured and yarn_tpool_exec should stop.

 */
typedef bool (*yarn_worker_t) (yarn_word_t, void*);


bool yarn_tpool_init ();
void yarn_tpool_destroy();

//! Executes the tasks and returns when everyone is done is called.
bool yarn_tpool_exec (yarn_worker_t worker, void* task);

yarn_word_t yarn_tpool_size ();
bool yarn_tpool_is_done();


#endif // YARN_TPOOL_H_
