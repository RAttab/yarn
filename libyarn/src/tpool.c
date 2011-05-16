/*!
\author Rémi Attab
\license FreeBSD (see LICENSE file)

Implementation details of threads.h

\todo pthread error checking.
 */

#include <tpool.h>

#include "helper.h"
#include "alloc.h"

#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <sched.h>
#include <unistd.h>
#include <stdio.h>


struct pool_thread {
  pthread_t thread;
};


//! The one and only thread pool.
static struct pool_thread* g_pool = NULL;
static yarn_tsize_t g_pool_size = 0;


struct pool_task {
  yarn_worker_t worker_fun;
  void* data;
};

static struct pool_task* volatile g_pool_task = NULL;
static volatile bool g_pool_task_error = false;
static pthread_mutex_t g_pool_task_lock;
static pthread_cond_t g_pool_task_cond;
static pthread_barrier_t g_pool_task_barrier;



static inline void* worker_launcher (void* param);


static inline yarn_tsize_t get_processor_count(void) {
  return sysconf(_SC_NPROCESSORS_ONLN);
}



yarn_tsize_t yarn_tpool_size (void) {
  return g_pool_size;
}


bool yarn_tpool_init (void) {
  if (g_pool != NULL) {
    return true;
  }

  int err = 0;

  err = pthread_cond_init(&g_pool_task_cond, NULL);
  if (err) goto cond_error;

  err = pthread_mutex_init(&g_pool_task_lock, NULL);
  if (err) goto mutex_error;

  g_pool_size = get_processor_count();

  g_pool = (struct pool_thread*) yarn_malloc(sizeof(struct pool_thread) * g_pool_size);
  if (g_pool == NULL) goto pool_alloc_error;

  yarn_tsize_t pool_id = 0;
  for (; pool_id < g_pool_size; ++pool_id) {
    
    err = pthread_create(
        &g_pool[pool_id].thread, NULL, worker_launcher, (void*) pool_id);
    if (err) goto thread_create_error;
    
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    //! \todo Might want to stagger the affinities to account for Hyperthreading.
    CPU_SET(pool_id, &cpuset);    
    YARN_CHECK_RET0(pthread_setaffinity_np(
        g_pool[pool_id].thread, sizeof(cpu_set_t), &cpuset));
  }
  
  return true;

  // Cleanup in case of error.

 thread_create_error:
  for (yarn_tsize_t i = 0; i < pool_id; ++i) {
    pthread_cancel(g_pool[i].thread);
    pthread_detach(g_pool[i].thread);
  }

  yarn_free(g_pool);
 pool_alloc_error:
  pthread_mutex_destroy(&g_pool_task_lock);
 mutex_error:
  pthread_cond_destroy(&g_pool_task_cond);
 cond_error:
  perror(__FUNCTION__);
  return false;
}


void yarn_tpool_destroy (void) {
  if (g_pool == NULL) {
    return;
  }

  for (yarn_tsize_t pool_id = 0; pool_id < g_pool_size; ++pool_id) {
    pthread_cancel(g_pool[pool_id].thread);
    pthread_join(g_pool[pool_id].thread, NULL);
  }

  pthread_cond_destroy(&g_pool_task_cond);
  pthread_mutex_destroy(&g_pool_task_lock);

  //! \todo May not have been initialized and destroying it could cause problems.
  pthread_barrier_destroy(&g_pool_task_barrier);

  yarn_free(g_pool);

  g_pool = NULL;
  g_pool_size = 0;
}


static inline void* worker_launcher (void* param);


//! Executes the tasks and returns when everyone is done or yarn_tpool_interrupt is called.
bool yarn_tpool_exec (yarn_worker_t worker, void* task) {

  int err = 0;
  
  struct pool_task* ptask = (struct pool_task*)yarn_malloc(sizeof(struct pool_task*));
  if (ptask == NULL) goto task_alloc_error;

  ptask->data = task;
  ptask->worker_fun = worker;

  // Posts the task and notify the worker threads.
  g_pool_task_error = false;
  g_pool_task = ptask;

  err = pthread_barrier_init(&g_pool_task_barrier, NULL, g_pool_size);
  if (err) goto barrier_error;

  YARN_CHECK_RET0(pthread_cond_signal(&g_pool_task_cond));

  // Wait for the worker threads to indicate that they're done and clean up.
  bool task_error = false;
  {
    YARN_CHECK_RET0(pthread_mutex_lock(&g_pool_task_lock));
    while (g_pool_task != NULL) {
      YARN_CHECK_RET0(pthread_cond_wait(&g_pool_task_cond, &g_pool_task_lock));
    }
    yarn_free(ptask);
    task_error = g_pool_task_error;
    YARN_CHECK_RET0(pthread_mutex_unlock(&g_pool_task_lock));
  }

  return !task_error;

 barrier_error:
  yarn_free(ptask);
 task_alloc_error:
  perror(__FUNCTION__);
  return false;

}



static inline void* worker_launcher (void* param) {
  const yarn_tsize_t pool_id = (yarn_tsize_t) param;

  while (true) {

    struct pool_task* task = NULL;

    {
      YARN_CHECK_RET0(pthread_mutex_lock(&g_pool_task_lock));
      while (g_pool_task == NULL) {
	YARN_CHECK_RET0(pthread_cond_wait(&g_pool_task_cond, &g_pool_task_lock));
      }
      task = g_pool_task;
      YARN_CHECK_RET0(pthread_mutex_unlock(&g_pool_task_lock));
    }

    bool ret = (*task->worker_fun)(pool_id, task->data);
    YARN_CHECK_RET0(pthread_barrier_wait(&g_pool_task_barrier));

    if(pool_id == 0) {
      g_pool_task_error = !ret;
      g_pool_task = NULL;
      YARN_CHECK_RET0(pthread_cond_signal(&g_pool_task_cond));
    }

    YARN_CHECK_RET0(pthread_testcancel());
  }


  return NULL;
}
