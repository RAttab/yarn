/*!
\author Rémi Attab
\license FreeBSD (see license file).
*/


#include "check_libyarn.h"

#include <types.h>
#include <epoch.h>
#include <tpool.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>


static void t_epoch_setup(void) {
  yarn_epoch_init();
}


static void t_epoch_teardown(void) {
  yarn_epoch_destroy();
}


#define check_status(epoch, expected)					\
  do {									\
    enum yarn_epoch_status status = yarn_epoch_get_status(epoch);	\
    fail_if(status != (expected), "status=%d, expected=%d", status, (expected)); \
  } while(false)


START_TEST(t_epoch_next) {
  const yarn_word_t IT_COUNT = sizeof(yarn_word_t)*8/2;

  for (yarn_word_t i = 0; i < IT_COUNT; ++i) {
    yarn_word_t next_epoch = yarn_epoch_next();
    fail_if(next_epoch != i, "next_epoch=%zu, i=%zu", next_epoch, i);
    check_status(next_epoch, yarn_epoch_waiting);
  }
}
END_TEST


START_TEST(t_epoch_rollback) {
  void* VALUE = (void*) 0xDEADBEEF;
  
  for (int i = 0; i < 8; ++i) {
    yarn_word_t epoch = yarn_epoch_next();
    yarn_epoch_set_data(epoch, VALUE);
    yarn_epoch_set_task(epoch, VALUE);
    yarn_epoch_set_executing(epoch);
  }

  yarn_epoch_do_rollback(4);

  for (yarn_word_t epoch = 4; epoch < 8; ++epoch) {
      yarn_word_t next_epoch = yarn_epoch_next();
      
      fail_if(next_epoch != epoch, "next_epoch=%zu, expected=%zu", next_epoch, epoch);
      check_status(epoch, yarn_epoch_rollback);

      void* data = yarn_epoch_get_data(epoch);
      fail_if(data != VALUE, "data=%p, expected=%p", data, VALUE);

      void* task = (void*) yarn_epoch_get_task(epoch);
      fail_if(task != VALUE, "task=%p, expected=%p", task, VALUE);
  }


  for (int i = 0; i < 8; ++i) {
    yarn_word_t next_epoch = yarn_epoch_next();
    check_status(next_epoch, yarn_epoch_waiting);
  }

}
END_TEST

START_TEST(t_epoch_commit) {
  const int IT_COUNT = sizeof(yarn_word_t)*8/2;
  const uintptr_t VALUE = 0xDEADBEEF;
  yarn_word_t epoch_to_commit;
  void* task;
  void* data;

  {
    bool ret = yarn_epoch_get_next_commit(&epoch_to_commit, &task, &data);
    fail_if(ret);
  }

  for (int j = 0; j < IT_COUNT; ++j) {

    for (int i = 0; i < IT_COUNT; ++i) {
      yarn_word_t next_epoch = yarn_epoch_next();
      yarn_epoch_set_task(next_epoch, (void*) VALUE);
      yarn_epoch_set_executing(next_epoch);
      yarn_epoch_set_done(next_epoch);

      check_status(next_epoch, yarn_epoch_done);
    }

    for (int i = 0; i < IT_COUNT; ++i) {
      bool ret = yarn_epoch_get_next_commit(&epoch_to_commit, &task, &data);

      fail_if(!ret);
      fail_if(task != (void*)VALUE, "task=%p, expected=%p", task, (void*) VALUE);
      check_status(epoch_to_commit, yarn_epoch_done);

      yarn_epoch_set_commit(epoch_to_commit);
    }

  }

  {
    bool ret = yarn_epoch_get_next_commit(&epoch_to_commit, &task, &data);
    fail_if(ret);
  }

}
END_TEST



static inline void waste_time() { 
  for (int j = 0; j < 100; ++j); 
}


static yarn_word_t find_safe_rollback_epoch(yarn_word_t start) {
  yarn_word_t to_rollback = start;

  for (yarn_word_t e = yarn_epoch_last()-1; e != start; --e) {
    enum yarn_epoch_status status = yarn_epoch_get_status(e);
    if (status == yarn_epoch_waiting) {
      break;
    }
    to_rollback = e;
  }

  return to_rollback;
}

// Simulates a basic epoch processing loop. Since the functions have to be used in a very
// specific way, we try to emulate actual use as much as possible.
//! \todo need to throw in some fail_*.
bool t_epoch_para_worker (yarn_word_t pool_id, void* task) {
  (void) pool_id;
  (void) task;

  for (int i = 0; i < 10000; ++i) {
    
    const yarn_word_t epoch = yarn_epoch_next();

    // Fake rollback phase.
    {
      enum yarn_epoch_status status = yarn_epoch_get_status(epoch);
      if (status == yarn_epoch_rollback) {
	waste_time();
	yarn_epoch_set_waiting(epoch);
      }
    }
    
    

    // Fake execution phase.
    {
      yarn_epoch_set_executing(epoch);
      waste_time();
      
      if (rand() % 5 == 0) {
	yarn_word_t to_rollback = find_safe_rollback_epoch(epoch);
	if (to_rollback != epoch) {
	  yarn_epoch_do_rollback(to_rollback);
	  continue;
	}
      }
      
      yarn_epoch_set_done(epoch);
    }

    // Fake commit phase
    {
      yarn_word_t to_commit;
      void* task;
      void* data;
      while(yarn_epoch_get_next_commit(&to_commit, &task, &data)) {
	waste_time();
	yarn_epoch_set_commit(to_commit);
      }
    }
  }

  return true;
}

START_TEST(t_epoch_para) {
  yarn_tpool_init();
  yarn_tpool_exec(t_epoch_para_worker, NULL);
  yarn_tpool_destroy();
}
END_TEST



Suite* yarn_epoch_suite (void) {
  Suite* s = suite_create("yarn_epoch");

  TCase* tc_basic = tcase_create("yarn_epoch.basic");
  tcase_add_checked_fixture(tc_basic, t_epoch_setup, t_epoch_teardown);
  tcase_add_test(tc_basic, t_epoch_next);
  tcase_add_test(tc_basic, t_epoch_rollback);
  tcase_add_test(tc_basic, t_epoch_commit);
  tcase_add_test(tc_basic, t_epoch_para);
  suite_add_tcase(s, tc_basic);

  return s;
}
