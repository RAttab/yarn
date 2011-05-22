/*!
\author Rémi Attab
\license FreeBSD (see the LICENSE file)

Implementation note:
We don't use yarn_timestamp_t since we need more flexibility with the epochs values. We
still need to compare them using yarn_timestamp_comp if we want to be able to increase
the epochs to infinity and still keep the comparaison coherent.
 */


#include "epoch.h"

#include <types.h>
#include "timestamp.h"
#include "atomic.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>



struct epoch_info {
  yarn_atomic_var status;
  void* task;
  void* data;
};


static struct epoch_info* g_epoch_list;
static const size_t g_epoch_list_size = sizeof(yarn_word_t)*8;

static yarn_atomic_var g_epoch_first;
static yarn_atomic_var g_epoch_next;


static inline struct epoch_info* get_epoch_info (yarn_word_t epoch) {
  size_t index = epoch % g_epoch_list_size;
  return &(g_epoch_list[index]);
}




bool yarn_epoch_init(void) {
  g_epoch_list = malloc(g_epoch_list_size * sizeof(struct epoch_info));
  if (!g_epoch_list) goto alloc_error;

  for (size_t i = 0; i < g_epoch_list_size; ++i) {
    yarn_writev(&g_epoch_list[i].status, yarn_epoch_commit);
  }
  
  yarn_writev(&g_epoch_first, 0);
  yarn_writev(&g_epoch_next, 0);
 
  return true;
 
  //free(g_epoch_list);
 alloc_error:
  perror(__FUNCTION__);
  return false;
}

void yarn_epoch_destroy(void) {
  free(g_epoch_list);
}






yarn_word_t yarn_epoch_first(void) {
  return yarn_readv(&g_epoch_first);
}

yarn_word_t yarn_epoch_last(void) {
  return yarn_readv(&g_epoch_next);
}

yarn_word_t yarn_epoch_next(void) {
  yarn_word_t cur_next = yarn_get_and_incv(&g_epoch_next);
  
  struct epoch_info* info = get_epoch_info(cur_next);

  // We caught up to g_epoch_first. Let's wait till it's committed.
  // Note that this shouldn't happen unless you have 30+ or 60+ cores on your CPU.
  yarn_spinv_neq(&info->status, yarn_epoch_waiting);
  yarn_spinv_neq(&info->status, yarn_epoch_executing);
  yarn_spinv_neq(&info->status, yarn_epoch_done);

  yarn_casv(&info->status, yarn_epoch_commit, yarn_epoch_waiting);

  return cur_next;
}


void yarn_epoch_do_rollback(yarn_word_t start) {
  yarn_word_t epoch_last = yarn_epoch_last();
  
  for (yarn_word_t epoch = start; yarn_timestamp_comp(epoch, epoch_last) < 0; ++epoch) {
    struct epoch_info* info = get_epoch_info(epoch);

    enum yarn_epoch_status status = yarn_readv(&info->status);
    assert(status == yarn_epoch_executing || 
	   status == yarn_epoch_done || 
	   status == yarn_epoch_rollback);

    yarn_writev_barrier(&info->status, yarn_epoch_rollback);

  }

  yarn_writev_barrier(&g_epoch_next, start);
}


bool yarn_epoch_get_next_commit(yarn_word_t* epoch, void** task, void** data) {
  yarn_word_t to_commit;
  struct epoch_info* info;

  // increment first if it has the correct status.
  do {
    to_commit = yarn_readv(&g_epoch_first);
    yarn_mem_barrier();
    yarn_word_t next = yarn_readv(&g_epoch_next);

    // We can tolerate false positives on this check.    
    if (to_commit == next) {
      return false;
    }
    
    info = get_epoch_info(to_commit);
    if (yarn_readv(&info->status) != yarn_epoch_done) {
      return false;
    }

  } while (yarn_casv_fast(&g_epoch_first, to_commit, to_commit+1) != to_commit);

  *task = info->task;
  info->task = NULL;

  *data = info->data;
  info->data = NULL;

  *epoch = to_commit;
  
  assert(yarn_readv(&info->status) == yarn_epoch_done);

  return true;
}

void yarn_epoch_set_commit(yarn_word_t epoch) {
  struct epoch_info* info = get_epoch_info(epoch);

  enum yarn_epoch_status old_status;
  old_status = yarn_casv(&info->status, yarn_epoch_done, yarn_epoch_commit);
  assert(old_status == yarn_epoch_done);
}

void yarn_epoch_set_done(yarn_word_t epoch) {
  struct epoch_info* info = get_epoch_info(epoch);
  
  enum yarn_epoch_status status = yarn_readv(&info->status);
  assert(status == yarn_epoch_executing || status == yarn_epoch_rollback);
  (void) status; // warning suppresion.

  yarn_casv(&info->status, yarn_epoch_executing, yarn_epoch_done);
}

void yarn_epoch_set_executing(yarn_word_t epoch) {
 struct epoch_info* info = get_epoch_info(epoch);
  
  enum yarn_epoch_status status = yarn_readv(&info->status);
  assert(status == yarn_epoch_waiting);
  (void) status; // warning suppresion.

  yarn_writev_barrier(&info->status, yarn_epoch_executing);
}

void yarn_epoch_set_waiting(yarn_word_t epoch) {
 struct epoch_info* info = get_epoch_info(epoch);
  
  enum yarn_epoch_status status = yarn_readv(&info->status);
  assert(status == yarn_epoch_rollback || status == yarn_epoch_commit);
  (void) status; // warning suppresion.

  yarn_writev_barrier(&info->status, yarn_epoch_waiting);
}



enum yarn_epoch_status yarn_epoch_get_status (yarn_word_t epoch) {
  struct epoch_info* info = get_epoch_info(epoch);
  return yarn_readv(&info->status);
}

void* yarn_epoch_get_task (yarn_word_t epoch) {
  struct epoch_info* info = get_epoch_info(epoch);
  return info->task;
}
void yarn_epoch_set_task (yarn_word_t epoch, void* task) {
  struct epoch_info* info = get_epoch_info(epoch);
  info->task = task;
}

void* yarn_epoch_get_data(yarn_word_t epoch) {
  struct epoch_info* info = get_epoch_info(epoch);
  return info->data;
}
void yarn_epoch_set_data(yarn_word_t epoch, void* data) {
  struct epoch_info* info = get_epoch_info(epoch);
  info->data = data;
}
