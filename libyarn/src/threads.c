/*!
\author Rémi Attab
\license FreeBSD (see LICENSE file)

Implementation details of threads.h

\todo pthread error checking.
 */

#include <threads.h>

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
static pthread_mutex_t g_pool_task_lock;
static pthread_cond_t g_pool_task_cond;
static pthread_barrier_t g_pool_task_barrier;



static inline void* worker_launcher (void* param);


static inline yarn_tsize_t get_processor_count() {
  return sysconf(_SC_NPROCESSORS_ONLN);
}



yarn_tsize_t yarn_tpool_size () {
  return g_pool_size;
}



void yarn_tpool_init () {
  if (g_pool != NULL) {
    return;
  }

  pthread_cond_init(&g_pool_task_cond, NULL);
  pthread_mutex_init(&g_pool_task_lock, NULL);

  g_pool_size = get_processor_count();
  g_pool = (struct pool_thread*) yarn_malloc(sizeof(struct pool_thread) * g_pool_size);
  
  for (yarn_tsize_t pool_id = 0; pool_id < g_pool_size; ++pool_id) {
    
    int ret = pthread_create(
        &g_pool[pool_id].thread, NULL, worker_launcher, (void*) pool_id);

    if (ret != 0) {
      perror("yarn_tpool_init.pthread_create");
      assert(false); //! \todo Need better error handling.
    }
    
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    //! \todo Might want to stagger the affinities to account for Hyperthreading.
    CPU_SET(pool_id, &cpuset);    
    pthread_setaffinity_np(g_pool[pool_id].thread, sizeof(cpu_set_t), &cpuset);
			   
  }

}

void yarn_tpool_cleanup () {
  if (g_pool == NULL) {
    return;
  }

  yarn_tpool_interrupt();

  for (yarn_tsize_t pool_id = 0; pool_id < g_pool_size; ++pool_id) {
    pthread_join(g_pool[pool_id].thread, NULL);
  }

  pthread_cond_destroy(&g_pool_task_cond);
  pthread_mutex_destroy(&g_pool_task_lock);
  pthread_barrier_destroy(&g_pool_task_barrier);

  yarn_free(g_pool);
  g_pool = NULL;
  g_pool_size = 0;
}


void yarn_tpool_interrupt () {
  if (g_pool == NULL) {
    return;
  }

  for (yarn_tsize_t pool_id = 0; pool_id < g_pool_size; ++pool_id) {
    pthread_cancel(g_pool[pool_id].thread);
  }
}




static inline void* worker_launcher (void* param);


//! Executes the tasks and returns when everyone is done or yarn_tpool_interrupt is called.
void yarn_tpool_exec (yarn_worker_t worker, void* task) {
  
  struct pool_task* ptask = (struct pool_task*)yarn_malloc(sizeof(struct pool_task*));
  ptask->data = task;
  ptask->worker_fun = worker;

  // Posts the task and notify the worker threads.
  g_pool_task = ptask;
  pthread_barrier_init(&g_pool_task_barrier, NULL, g_pool_size);
  pthread_cond_signal(&g_pool_task_cond);

  // Wait for the worker threads to indicate that they're done and clean up.
  {
    pthread_mutex_lock(&g_pool_task_lock);
    while (g_pool_task != NULL) {
      pthread_cond_wait(&g_pool_task_cond, &g_pool_task_lock);
    }
    yarn_free(ptask);
    pthread_mutex_unlock(&g_pool_task_lock);
  }

}



static inline void* worker_launcher (void* param) {
  const yarn_tsize_t pool_id = (yarn_tsize_t) param;

  while (true) {

    struct pool_task* task = NULL;

    {
      pthread_mutex_lock(&g_pool_task_lock);
      while (g_pool_task == NULL) {
	pthread_cond_wait(&g_pool_task_cond, &g_pool_task_lock);
      }
      task = g_pool_task;
      pthread_mutex_unlock(&g_pool_task_lock);
    }

    (*task->worker_fun)(pool_id, task->data);
    pthread_barrier_wait(&g_pool_task_barrier);

    if(pool_id == 0) {
      g_pool_task = NULL;
      pthread_cond_signal(&g_pool_task_cond);
    }

    pthread_testcancel();
  }


  return NULL;
}
