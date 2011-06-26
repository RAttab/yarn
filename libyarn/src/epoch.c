/*!
\author Rémi Attab
\license FreeBSD (see the LICENSE file)

Implementation note:
We don't use yarn_timestamp_t since we need more flexibility with the epochs values. We
still need to compare them using yarn_timestamp_comp if we want to be able to increase
the epochs to infinity and still keep the comparaison coherent.
 */


#include "epoch.h"

#include <yarn/types.h>
#include "helper.h"
#include "tpool.h"
#include "timestamp.h"
#include "atomic.h"
#include "bits.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#define YARN_DBG 0
#include "dbg.h"



struct epoch_info {
  yarn_atomic_var status;
  void* task;
};


static yarn_word_t g_epoch_max;

static struct epoch_info* g_epoch_list;

// Cursors for the g_epoch_list.
static yarn_atomic_var g_epoch_first;
static yarn_atomic_var g_epoch_next;
static yarn_atomic_var g_epoch_next_commit;

// Bitfield that keeps track of the all the rolledback epochs.
static yarn_atomic_var g_rollback_flag;

// Prevents a call to rollback from being executed while calling next.
// Still allows multiple calls the next at the same time but with only one
// active for a given epoch.
static pthread_rwlock_t g_rollback_lock;

// Indicates an epoch that stops the calculations.
static yarn_atomic_var g_epoch_stop;



static inline bool is_stop_set(yarn_word_t stop_epoch);
static inline void rollback_stop (yarn_word_t rollback_epoch);
static inline void update_stop ();



static inline size_t get_epoch_index (yarn_word_t epoch) {
  return YARN_BIT_INDEX(epoch, g_epoch_max);
}

static inline struct epoch_info* get_epoch_info (yarn_word_t epoch) {
  return &(g_epoch_list[get_epoch_index(epoch)]);
}




bool yarn_epoch_init(void) {
  int ret = pthread_rwlock_init(&g_rollback_lock, NULL);
  if (ret) goto lock_error;

  g_epoch_max = yarn_epoch_max();

  g_epoch_list = malloc(g_epoch_max * sizeof(struct epoch_info));
  if (!g_epoch_list) goto alloc_error;

  yarn_epoch_reset();

  return true;
 
  //free(g_epoch_list);
 alloc_error:
  pthread_rwlock_destroy(&g_rollback_lock);
 lock_error:
  perror(__FUNCTION__);
  return false;
}

bool yarn_epoch_reset(void) {
  assert(g_epoch_max == yarn_epoch_max() && "tpool_size() changed!");

  for (size_t i = 0; i < g_epoch_max; ++i) {
    yarn_writev(&g_epoch_list[i].status, yarn_epoch_commit);
    g_epoch_list[i].task = NULL;
  }

  yarn_writev(&g_epoch_first, 0);
  yarn_writev(&g_epoch_next, 0);
  yarn_writev(&g_epoch_next_commit, 0);
  yarn_writev(&g_rollback_flag, 0);
  yarn_writev(&g_epoch_stop, -1);  

  return true;
}

void yarn_epoch_destroy(void) {
  free(g_epoch_list);
  pthread_rwlock_destroy(&g_rollback_lock);
}




yarn_word_t yarn_epoch_max(void) {
  yarn_word_t max_size = YARN_WORD_BIT_SIZE;
  yarn_word_t optimal_size = yarn_tpool_size()*2;

  return optimal_size < max_size ? optimal_size : max_size;
}


yarn_word_t yarn_epoch_first(void) {
  return yarn_readv(&g_epoch_first);
}

yarn_word_t yarn_epoch_last(void) {
  return yarn_readv(&g_epoch_next);
}

static inline bool inc_epoch_next (yarn_word_t* next_epoch) {
  int attempts = 0;
  (void) attempts; // Warning suppression.

  yarn_word_t cur_next;
  bool retry = false;
  do {
    //assert(attempts++ <= 20);

    cur_next = yarn_readv(&g_epoch_next);
    yarn_word_t first = yarn_readv(&g_epoch_first);
    retry = false;

    // If we've reached our own tail then spin until it unblocks.
    // Note that if this happens then there's something wrong with the thread scheduling
    // or one of the threads is taking forever to finish.
    // So something has gone wrong and yield can alleviate scheduling issues.
    if (cur_next != first && get_epoch_index(cur_next) == get_epoch_index(first)) {
      retry = true;
    }

    struct epoch_info* info = get_epoch_info(cur_next);
    if (!retry) {
      enum yarn_epoch_status status = yarn_readv(&info->status);
      if (status == yarn_epoch_pending_rollback) {
	retry = true;
      }
    }

    if (!retry) {
      const yarn_word_t stop_epoch = yarn_readv(&g_epoch_stop);
      const yarn_word_t first_epoch = yarn_readv(&g_epoch_first);

      if (is_stop_set(stop_epoch) && yarn_timestamp_comp(cur_next, stop_epoch) >= 0) {
	if (stop_epoch == first_epoch) {
	  return false;
	}

	else {
	  retry = true;
	}
      }
    }

    if (retry) {
      YARN_CHECK_RET0(pthread_rwlock_unlock(&g_rollback_lock));
      pthread_yield();
      YARN_CHECK_RET0(pthread_rwlock_rdlock(&g_rollback_lock));
      continue;
    }
    
    enum yarn_epoch_status status = yarn_readv(&info->status);
    (void) status; // warning suppression.
    DBG printf("\t\t\t\t\t\t[%zu] - INC - status=%d\n", cur_next, status);

  } while (retry || yarn_casv(&g_epoch_next, cur_next, cur_next+1) != cur_next);

  *next_epoch = cur_next;
  return true;
}

bool yarn_epoch_next(yarn_word_t* next_epoch, enum yarn_epoch_status* old_status) {

  YARN_CHECK_RET0(pthread_rwlock_rdlock(&g_rollback_lock));
  DBG printf("\t\t\t\t\t\tNEXT - LOCK\n");

  bool ret = inc_epoch_next(next_epoch);
  if (ret) {
    struct epoch_info* info = get_epoch_info(*next_epoch);
    
    *old_status = yarn_readv(&info->status);
    yarn_writev(&info->status, yarn_epoch_executing);
    
    DBG printf("[%zu] - EXECUTING - old_status=%d\n", *next_epoch, *old_status);
    assert(*old_status == yarn_epoch_commit || *old_status == yarn_epoch_rollback);
    
  }

  DBG printf("\t\t\t\t\t\tNEXT - UNLOCK\n");
  YARN_CHECK_RET0(pthread_rwlock_unlock(&g_rollback_lock));

  return ret;
}



static inline bool is_stop_set(yarn_word_t stop_epoch) {
  yarn_word_t first = yarn_readv(&g_epoch_first);
  return yarn_timestamp_comp(stop_epoch, first) >= 0;
}


void yarn_epoch_stop(yarn_word_t stop_epoch) {
  yarn_word_t old_stop;
  yarn_word_t new_stop;
  bool is_set;

  do {
    old_stop = yarn_readv(&g_epoch_stop);

    // stop is used like an end bound. 
    // This is to keep it consistent with first which simplifies things.
    new_stop = stop_epoch+1;

    is_set = is_stop_set(old_stop);
    if (is_set && yarn_timestamp_comp(old_stop, new_stop) < 0) {
      return;
    }

  } while (yarn_casv(&g_epoch_stop, old_stop, new_stop) != old_stop);

  DBG printf("\t\t\t\t\t\t\t\tSTOP_SET[%zu] =END= old=%zu, is_set=%d\n", 
	 new_stop, old_stop, is_set);
}

static inline void rollback_stop (yarn_word_t rollback_epoch) {
  yarn_word_t old_stop;
  yarn_word_t new_stop;
  bool is_set;

  do {
    old_stop = yarn_readv(&g_epoch_stop);
    
    is_set = is_stop_set(old_stop);
    if (!is_set) {
      return;
    }
    if (yarn_timestamp_comp(old_stop, rollback_epoch) <= 0) {
      return;
    }

    new_stop = yarn_readv(&g_epoch_first) -1;
  } while (yarn_casv(&g_epoch_stop, old_stop, new_stop) != old_stop);

  DBG printf("\t\t\t\t\t\t\t\tSTOP_ROLLBACK[%3zu] =END= old=%zu, is_set%d\n", 
	 new_stop, old_stop, is_set);
}

/*
  Keeps the g_epoch_stop close before g_epoch_first. 
  This is to avoid problems if the epochs overflow.
*/
static inline void update_stop () {
  yarn_word_t old_stop;
  yarn_word_t new_stop;
  bool is_set;

  do {
    old_stop = yarn_readv(&g_epoch_stop);

    is_set = is_stop_set(old_stop);
    if (is_set) {
      return;
    }
    
    new_stop = yarn_readv(&g_epoch_first) -1;
  } while(yarn_casv(&g_epoch_stop, old_stop, new_stop) != old_stop);

  DBG printf("\t\t\t\t\t\t\t\tSTOP_UPDATE[%3zu] =END= old=%zu, is_set=%d\n", 
	 new_stop, old_stop, is_set);
}


void yarn_epoch_do_rollback(yarn_word_t start) {

  YARN_CHECK_RET0(pthread_rwlock_wrlock(&g_rollback_lock));
  DBG printf("\t\t\t\t\t\tROLLBACK - LOCK\n");

  rollback_stop(start);
  
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
      
      case yarn_epoch_done:
	new_status = yarn_epoch_rollback;
	break;
      default:
	DBG printf("[%zu] - DO_ROLLBACK - ERROR - old_status=%d\n", epoch, old_status);
	assert(false);
      }

      if (skip_epoch) {
	break;
      }      

    } while(yarn_casv(&info->status, old_status, new_status) != old_status);

    if (skip_epoch) {
      continue;
    }
    else {
      DBG printf("[%zu] - DO_ROLLBACK - old_status=%d, new_status=%d\n", 
		 epoch, old_status, new_status);

      // Update the rollbackflag.
      yarn_word_t old_flag;
      yarn_word_t new_flag;
      do {
	old_flag = yarn_readv(&g_rollback_flag);
	new_flag = YARN_BIT_SET(old_flag, epoch, g_epoch_max);
      } while (yarn_casv(&g_rollback_flag, old_flag, new_flag) != old_flag);

      DBG printf("[---] ROLLBACK -> SET [%3zu] - flag="YARN_SHEX"\n", 
	     epoch, YARN_AHEX(new_flag));

    }
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

  DBG printf("\t\t\t\t\t\tROLLBACK - UNLOCK\n");
  YARN_CHECK_RET0(pthread_rwlock_unlock(&g_rollback_lock));
}

void yarn_epoch_rollback_done(yarn_word_t epoch) {
  yarn_word_t old_flag;
  yarn_word_t new_flag;
  do {
    old_flag = yarn_readv(&g_rollback_flag);
    new_flag = YARN_BIT_CLEAR(old_flag, epoch, g_epoch_max);
  } while (yarn_casv(&g_rollback_flag, old_flag, new_flag) != old_flag);

  DBG printf("[---] ROLLBACK -> CLEAR [%3zu] - flag="YARN_SHEX"\n", 
	 epoch, YARN_AHEX(new_flag));

}


bool yarn_epoch_get_next_commit(yarn_word_t* epoch, void** task) {
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

    yarn_word_t stop_epoch = yarn_readv(&g_epoch_stop);    
    if (is_stop_set(stop_epoch) && stop_epoch == to_commit) {
      return false;
    }
      
  } while (yarn_casv(&g_epoch_next_commit, to_commit, to_commit+1) != to_commit);

  *task = info->task;
  info->task = NULL;

  *epoch = to_commit;
  
  assert(yarn_readv(&info->status) == yarn_epoch_done);

  return true;
}

void yarn_epoch_commit_done(yarn_word_t epoch) {
  struct epoch_info* info = get_epoch_info(epoch);

  enum yarn_epoch_status old_status = yarn_readv(&info->status);
  DBG printf("[%zu] - COMMIT - old_status=%d\n", epoch, old_status);

  assert(old_status == yarn_epoch_done);
  (void) old_status; // Warning supression.

  yarn_writev_barrier(&info->status, yarn_epoch_commit);

  // Move the g_epoch_first as far as we can
  yarn_word_t old_first;
  yarn_word_t old_commit;
  while(true) {
    old_first = yarn_readv(&g_epoch_first);

    old_commit = yarn_readv(&g_epoch_next_commit);
    if (old_first == old_commit) {
      break;
    }

    enum yarn_epoch_status status = yarn_epoch_get_status(old_first);
    if (status != yarn_epoch_commit) {
      break;
    }
    
    // Increment the value. Doesn't matter if it fails or not.
    yarn_casv(&g_epoch_first, old_first, old_first+1);
  }

  update_stop();
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
      DBG printf("[%zu] - DONE - ERROR - old_status=%d\n", epoch, old_status);
      assert(false && (
	     old_status != yarn_epoch_executing ||
	     old_status != yarn_epoch_pending_rollback));
    }
  } while (yarn_casv(&info->status, old_status, new_status) != old_status);

  DBG printf("[%zu] - DONE - old_status=%d, new_status=%d\n", 
	     epoch, old_status, new_status);
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


yarn_word_t yarn_epoch_rollback_flags(void) {
  return yarn_readv(&g_rollback_flag);
}
