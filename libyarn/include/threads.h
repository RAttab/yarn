/*!
\author Rémi Attab
\license FreeBSD (see LICENSE file)

Header for all thread related primitives.
*/


#ifndef YARN_THREAD_H_
#define YARN_THREAD_H_


#include <stdint.h>
#include <stdbool.h>


//! Types to contain a number of threads.
typedef uint_fast16_t yarn_tsize_t;

/*!
\param uint_fast16_t The id of the thread in the pool. This number is easier to use then
the thread id provided by pthread because it's in the range 0 to yarn_tpool_size().

\param void* the task information passed to yarn_tpool_exec.

\return TBD.

 */
typedef void (*yarn_worker_t) (yarn_tsize_t, void*);


void yarn_tpool_init ();
void yarn_tpool_cleanup();

//! Executes the tasks and returns when everyone is done or yarn_tpool_interrupt is called.
void yarn_tpool_exec (yarn_worker_t worker, void* task);
void yarn_tpool_interrupt ();

yarn_tsize_t yarn_tpool_size ();
bool yarn_tpool_is_done();


#endif // YARN_THREAD_H_
