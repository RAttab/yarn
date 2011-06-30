/*!
\author Rémi Attab
\license FreeBSD (see the LICENSE file)

The main yarn header for the target programs.
 */

#include <yarn.h>

#include <yarn/dependency.h>
#include "tpool.h"
#include "epoch.h"
#include "bits.h"
#include "pmem.h"

#include <stdio.h>


struct task_info {
  yarn_executor_t executor;
  void* data;
};


static bool g_is_dep_init;


static bool init_dep (size_t ws_size, yarn_word_t index_size) {
  if (g_is_dep_init) {
    bool ret = yarn_dep_global_reset(ws_size, index_size);
    if (ret) {
      return true;
    }
    else {
      yarn_dep_global_destroy();
      perror(__FUNCTION__);

      // Try to recover with a regular init if not in debug mode.
      assert(false && "dep_reset failed.");
    }
  }

  bool ret = yarn_dep_global_init(ws_size, index_size);
  if (!ret) goto init_error;

  g_is_dep_init = true;

  return true;
  
 init_error:
  g_is_dep_init = false;  
  perror(__FUNCTION__);
  return false;
}

static void destroy_dep (void) {
  if (g_is_dep_init) {
    yarn_dep_global_destroy();
  }
}


bool yarn_init (void) {

  if (!yarn_tpool_init()) goto tpool_error;  
  if (!yarn_epoch_init()) goto epoch_error;

  return true;

  yarn_epoch_destroy();
 epoch_error:
  yarn_tpool_destroy();
 tpool_error:
  perror(__FUNCTION__);
  return false;
}


void yarn_destroy(void) {
  destroy_dep();
  yarn_epoch_destroy();
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


yarn_word_t yarn_thread_count() {
  return yarn_tpool_size();
}


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
  bool ret;

  ret = init_dep(ws_size, index_size);
  if (!ret) goto dep_alloc_error;

  ret = yarn_epoch_reset();
  if (!ret) goto epoch_reset_error;

  struct task_info info = {executor, data};
  ret = yarn_tpool_exec(pool_worker_simple, (void*) &info);
  if (!ret) goto exec_error;

  return true;

 exec_error:
 epoch_reset_error:
 dep_alloc_error:
  perror(__FUNCTION__);
  return false;
}
