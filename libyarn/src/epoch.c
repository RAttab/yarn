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
#include <helper.h>
#include "timestamp.h"
#include "atomic.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>


//#define YARN_EPOCH_DEBUG
#ifdef YARN_EPOCH_DEBUG
#  ifndef NDEBUG
#    define DBG(x) do{x;}while(false)
#  else
#    define DBG(x) ((void)0)
#  endif
#else
#  define DBG(X) ((void)0)
#endif



struct epoch_info {
  yarn_atomic_var status;
  void* task;
  void* data;
};


static struct epoch_info* g_epoch_list;
static const size_t g_epoch_list_size = sizeof(yarn_word_t)*8;

static yarn_atomic_var g_epoch_first;
static yarn_atomic_var g_epoch_next;
static yarn_atomic_var g_epoch_next_commit;

// Prevents a call to rollback from being executed while calling next.
// Still allows multiple calls the next at the same time but with only one
// active for a given epoch.
static pthread_rwlock_t g_rollback_lock;



static inline size_t get_epoch_index (yarn_word_t epoch) {
  return epoch % g_epoch_list_size;
}

static inline struct epoch_info* get_epoch_info (yarn_word_t epoch) {
  return &(g_epoch_list[get_epoch_index(epoch)]);
}




bool yarn_epoch_init(void) {
  int ret = pthread_rwlock_init(&g_rollback_lock, NULL);
  if (ret) goto lock_error;

  g_epoch_list = malloc(g_epoch_list_size * sizeof(struct epoch_info));
  if (!g_epoch_list) goto alloc_error;

  for (size_t i = 0; i < g_epoch_list_size; ++i) {
    yarn_writev(&g_epoch_list[i].status, yarn_epoch_commit);
    g_epoch_list[i].task = NULL;
    g_epoch_list[i].data = NULL;
  }
  
  yarn_writev(&g_epoch_first, 0);
  yarn_writev(&g_epoch_next, 0);
  yarn_writev(&g_epoch_next_commit, 0);
 
  return true;
 
  //free(g_epoch_list);
 alloc_error:
  pthread_rwlock_destroy(&g_rollback_lock);
 lock_error:
  perror(__FUNCTION__);
  return false;
}

void yarn_epoch_destroy(void) {
  free(g_epoch_list);
  pthread_rwlock_destroy(&g_rollback_lock);
}






yarn_word_t yarn_epoch_first(void) {
  return yarn_readv(&g_epoch_first);
}

yarn_word_t yarn_epoch_last(void) {
  return yarn_readv(&g_epoch_next);
}



static inline yarn_word_t inc_epoch_next () {
  yarn_word_t cur_next;
  bool retry = false;
  do {
    cur_next = yarn_readv(&g_epoch_next);
    yarn_word_t first = yarn_readv(&g_epoch_first);
    retry = false;

    // If we've reached our own tail then spin until it unblocks.
    // Note that if this happens then there's something wrong with the thread scheduling
    // or one of the threads is taking forever to finish.
    // So something has gone wrong and yield can alleviate scheduling issues.
    if (cur_next != first && get_epoch_index(cur_next) == get_epoch_index(first)) {
      // printf("\t\t\t\t\t\t[%zu] - INC -> TAIL - first=%zu\n", cur_next, first);

      YARN_CHECK_RET0(pthread_rwlock_unlock(&g_rollback_lock));
      pthread_yield();
      YARN_CHECK_RET0(pthread_rwlock_rdlock(&g_rollback_lock));

      retry = true;
      continue;
    }

    // The epoch is not yet ready so wait for it.
    struct epoch_info* info = get_epoch_info(cur_next);
    if (yarn_readv(&info->status) == yarn_epoch_pending_rollback) {
      /*
      enum yarn_epoch_status status = yarn_readv(&info->status);
      printf("\t\t\t\t\t\t[%zu] - INC -> PENDING - first=%zu, status=%d\n", 
	     cur_next, first, status);
      */
      YARN_CHECK_RET0(pthread_rwlock_unlock(&g_rollback_lock));
      pthread_yield();
      YARN_CHECK_RET0(pthread_rwlock_rdlock(&g_rollback_lock));

      retry = true;
      continue;
    }

    DBG(printf("\t\t\t\t\t\t[%zu] - INC - status=%d\n", cur_next, status));
  } while (retry || yarn_casv(&g_epoch_next, cur_next, cur_next+1) != cur_next);
  
  return cur_next;
}

yarn_word_t yarn_epoch_next(enum yarn_epoch_status* old_status) {

  YARN_CHECK_RET0(pthread_rwlock_rdlock(&g_rollback_lock));
  DBG(printf("\t\t\t\t\t\tNEXT - LOCK\n"));

  yarn_word_t cur_next = inc_epoch_next();
  
  struct epoch_info* info = get_epoch_info(cur_next);

  *old_status = yarn_readv(&info->status);
  yarn_writev(&info->status, yarn_epoch_waiting);

  DBG(printf("[%zu] - WAITING - old_status=%d\n", cur_next, *old_status));
  assert(*old_status == yarn_epoch_commit || *old_status == yarn_epoch_rollback);

  DBG(printf("\t\t\t\t\t\tNEXT - UNLOCK\n"));
  YARN_CHECK_RET0(pthread_rwlock_unlock(&g_rollback_lock));

  return cur_next;
}


void yarn_epoch_do_rollback(yarn_word_t start) {

  YARN_CHECK_RET0(pthread_rwlock_wrlock(&g_rollback_lock));
  DBG(printf("\t\t\t\t\t\tROLLBACK - LOCK\n"));

  
  yarn_word_t epoch_last = yarn_epoch_last();
  
  for (yarn_word_t epoch = start; yarn_timestamp_comp(epoch, epoch_last) < 0; ++epoch) {
    struct epoch_info* info = get_epoch_info(epoch);

    bool skip_epoch = false;
    enum yarn_epoch_status old_status;
    enum yarn_epoch_status new_status;

    // Set the rollback status depending on the current status.
    do {
      old_status = yarn_readv(&info->status);

      switch (old_status) {

      case yarn_epoch_commit:
	// There's a small window in next() between when the g_epoch_next is 
	// incremented and when the commit status is set to waiting.
	// In that window the epoch will have a valid commit state.
      case yarn_epoch_rollback:
      case yarn_epoch_pending_rollback:
	skip_epoch = true;
	break;
      case yarn_epoch_executing:
	new_status = yarn_epoch_pending_rollback;
	break;
      
      case yarn_epoch_waiting:
      case yarn_epoch_done:
	new_status = yarn_epoch_rollback;
	break;
      default:
	DBG(printf("[%zu] - DO_ROLLBACK - ERROR - old_status=%d\n", epoch, old_status));
	assert(false);
      }

      if (skip_epoch) {
	break;
      }

    } while(yarn_casv(&info->status, old_status, new_status) != old_status);

    if (skip_epoch) {
      continue;
    }

    DBG(printf("[%zu] - DO_ROLLBACK - old_status=%d, new_status=%d\n", 
	       epoch, old_status, new_status));
  }

  // Reset next to re-execute the rollback values.
  // I THINK this can be simplified because it owns the lock.
  yarn_word_t old_next;
  do {
    old_next = yarn_readv(&g_epoch_next);
    if (yarn_timestamp_comp(old_next, start) <= 0) {
      break;
    }
  } while(yarn_casv(&g_epoch_next, old_next, start) != old_next);

  DBG(printf("\t\t\t\t\t\tROLLBACK - UNLOCK\n"));
  YARN_CHECK_RET0(pthread_rwlock_unlock(&g_rollback_lock));
}


bool yarn_epoch_get_next_commit(yarn_word_t* epoch, void** task, void** data) {
  yarn_word_t to_commit;
  struct epoch_info* info;

  // increment first if it has the correct status.
  do {

    to_commit = yarn_readv(&g_epoch_next_commit);
    yarn_mem_barrier();
    yarn_word_t next = yarn_readv(&g_epoch_next);

    // We can tolerate false positives on this check.    
    if (to_commit == next) {
      return false;
    }

    info = get_epoch_info(to_commit);
    enum yarn_epoch_status status = yarn_readv(&info->status);
    if (status != yarn_epoch_done) {
      return false;
    }

  } while (yarn_casv(&g_epoch_next_commit, to_commit, to_commit+1) != to_commit);

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

  enum yarn_epoch_status old_status = yarn_readv(&info->status);
  DBG(printf("[%zu] - COMMIT - old_status=%d\n", epoch, old_status));

  assert(old_status == yarn_epoch_done);
  (void) old_status; // Warning supression.

  yarn_writev_barrier(&info->status, yarn_epoch_commit);

  // Move the g_epoch_first as far as we can
  yarn_word_t old_first;
  yarn_word_t old_commit;
  do {
    old_first = yarn_readv(&g_epoch_first);

    old_commit = yarn_readv(&g_epoch_next_commit);
    if (old_first == old_commit) {
      break;
    }

    enum yarn_epoch_status status = yarn_epoch_get_status(old_first);
    if (status != yarn_epoch_commit) {
      break;
    }
  } while (yarn_casv(&g_epoch_first, old_first, old_first+1));

}

void yarn_epoch_set_done(yarn_word_t epoch) {
  struct epoch_info* info = get_epoch_info(epoch);
  
  enum yarn_epoch_status old_status;
  enum yarn_epoch_status new_status;

  do {
    old_status = yarn_readv(&info->status);

    switch (old_status) {

    case yarn_epoch_executing:
      new_status = yarn_epoch_done;
      break;
    case yarn_epoch_pending_rollback:
      new_status = yarn_epoch_rollback;
      break;
    default:
      DBG(printf("[%zu] - DONE - ERROR - old_status=%d\n", epoch, old_status));
      assert(false && (
	     old_status != yarn_epoch_executing ||
	     old_status != yarn_epoch_pending_rollback));
    }
  } while (yarn_casv(&info->status, old_status, new_status) != old_status);

  DBG(printf("[%zu] - DONE - old_status=%d, new_status=%d\n", 
	     epoch, old_status, new_status));
}

bool yarn_epoch_set_executing(yarn_word_t epoch) {
  struct epoch_info* info = get_epoch_info(epoch);

  // Ensures that only one thread can execute an epoch at a time.
  // That also means that a thread see it's epoch highjacked by another thread who was
  // quicker.
  enum yarn_epoch_status old_status;
  old_status = yarn_casv(&info->status, yarn_epoch_waiting, yarn_epoch_executing);

  DBG(printf("[%zu] - EXECUTING if WAITING - old_status=%d\n", epoch, old_status));
  return old_status == yarn_epoch_waiting;
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
