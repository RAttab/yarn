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
  yarn_word_t cur_next;

  do {
    cur_next = yarn_readv(&g_epoch_next);
  } while (yarn_casv(&g_epoch_next, cur_next, cur_next+1));
  
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

    yarn_writev_barrier(&info->status, yarn_epoch_rollback);

    if(epoch == start) {
      yarn_writev(&g_epoch_next, start);
    }
  }
}


yarn_word_t yarn_epoch_do_commit_first(void** task, void** data) {


  yarn_word_t to_commit = yarn_get_and_incv(&g_epoch_first);

  struct epoch_info* info = get_epoch_info(to_commit);
  
  *task = info->task;
  info->task = NULL;

  *data = info->data;
  info->data = NULL;
  
  // Make sure the data was properly cleaned up.
  assert(yarn_readv(&info->status) == yarn_epoch_done);

  yarn_writev_barrier(&info->status, yarn_epoch_commit);

  return to_commit;
}



enum yarn_epoch_status yarn_epoch_get_status (yarn_word_t epoch) {
  struct epoch_info* info = get_epoch_info(epoch);
  return yarn_readv(&info->status);
}

void yarn_epoch_set_status(yarn_word_t epoch, enum yarn_epoch_status status) {
  struct epoch_info* info = get_epoch_info(epoch);
  yarn_writev(&info->status, status);
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
