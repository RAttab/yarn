/*!
\author Rémi Attab
\license FreeBSD (see the LICENSE file)

The main yarn header for the target programs.
 */

#include "yarn.h"

#include "tpool.h"
#include "epoch.h"
#include "dependency.h"
#include "bits.h"
#include "pmem.h"

#include <stdio.h>


struct task_info {
  yarn_executor_t executor;
  void* data;
};


static yarn_word_t g_base_epoch;

static struct yarn_pmem* g_task_allocator;

bool task_constructor (void* task) {
  (void) task;
  return true;
}
void task_destructor (void* task) {
  (void) task;
}


bool yarn_init (void) {

  if (!yarn_tpool_init()) 
    goto tpool_error;  

  g_task_allocator = yarn_pmem_init(sizeof(yarn_word_t),
				    task_constructor,
				    task_destructor);
  if (!g_task_allocator)
    goto allocator_error;


  if (!yarn_epoch_init()) 
    goto epoch_error;


  return true;

  yarn_epoch_destroy();
 epoch_error:
  yarn_pmem_destroy(g_task_allocator);
 allocator_error:
  yarn_tpool_destroy();
 tpool_error:
  perror(__FUNCTION__);
  return false;
}

void yarn_destroy(void) {
  yarn_epoch_destroy();
  yarn_pmem_destroy(g_task_allocator);
  yarn_tpool_destroy();
}


// Everytime we double the number of epoch we do an extra iteration per epoch.
/* This won't work too well if you have 
yarn_word_t epoch_task_count (yarn_word_t epoch) {
  const yarn_word_t divider = 32;

  yarn_word_t rel_epoch = epoch - g_base_epoch;
  return 1 + yarn_bit_log2(rel_epoch / divider);
}
*/



bool pool_worker_simple (yarn_word_t pool_id, void* task) {
  struct task_info* info = (struct task_info*) task;

  while (true) {
    enum yarn_epoch_status old_status;
    yarn_word_t epoch;
    if (!yarn_epoch_next(&epoch, &old_status)) {
      break;
    }

    if (old_status == yarn_epoch_rollback) {
      yarn_dep_rollback(epoch);
      yarn_epoch_rollback_done(epoch);
    }

    bool init_ok = yarn_dep_thread_init(pool_id, epoch);
    if (!init_ok) {
      goto init_error;
    }
    
    enum yarn_ret exec_ret = info->executor(pool_id, info->data);
    if (exec_ret == yarn_ret_break) {
      yarn_epoch_stop(epoch);
    }
    else if (exec_ret == yarn_ret_error) {
      goto exec_error;
    }
    
    yarn_epoch_set_done(epoch);
    yarn_dep_thread_destroy(pool_id);

    yarn_word_t commit_epoch;
    void* task;
    while(yarn_epoch_get_next_commit(&commit_epoch, &task)) {
      yarn_dep_commit(commit_epoch);
      yarn_epoch_commit_done(commit_epoch);
    }

  } // while;

  return true;

 exec_error:
  yarn_dep_thread_destroy(pool_id);
 init_error:
  perror(__FUNCTION__);
  return false;
}


bool yarn_exec_simple (yarn_executor_t executor, 
		      void* data, 
		      yarn_word_t ws_size, 
		      yarn_word_t index_size) 
{
  g_base_epoch = yarn_epoch_last();

  bool ret;
  if (!yarn_dep_global_init(ws_size, index_size)) {
    ret = false;
    goto dep_error;
  }

  struct task_info info = {executor, data};
  ret = yarn_tpool_exec(pool_worker_simple, (void*) &info);

 dep_error:
  yarn_dep_global_destroy();

  return ret;
}
