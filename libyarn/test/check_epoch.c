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


#define check_status(status,expected)					         \
  do {									         \
    fail_if(status != (expected), "status=%d, expected=%d", status, (expected)); \
  } while (false)
  

#define check_epoch_status(epoch,expected)			  \
  do {								  \
    enum yarn_epoch_status status = yarn_epoch_get_status(epoch); \
    check_status(status,(expected));                              \
  } while(false)


START_TEST(t_epoch_next) {
  const yarn_word_t IT_COUNT = sizeof(yarn_word_t)*8/2;
  
  for (yarn_word_t i = 0; i < IT_COUNT; ++i) {
    enum yarn_epoch_status old_status;
    yarn_word_t next_epoch = yarn_epoch_next(&old_status);
    fail_if(next_epoch != i, "next_epoch=%zu, i=%zu", next_epoch, i);
    check_epoch_status(next_epoch, yarn_epoch_waiting);
    check_status(old_status, yarn_epoch_commit);
  }
}
END_TEST


START_TEST(t_epoch_rollback) {
  void* VALUE = (void*) 0xDEADBEEF;
  
  for (int i = 0; i < 8; ++i) {
    enum yarn_epoch_status old_status;
    yarn_word_t epoch = yarn_epoch_next(&old_status);
    yarn_epoch_set_data(epoch, VALUE);
    yarn_epoch_set_task(epoch, VALUE);
    yarn_epoch_set_executing(epoch);
    yarn_epoch_set_done(epoch);
  }

  yarn_epoch_do_rollback(4);

  for (yarn_word_t epoch = 4; epoch < 8; ++epoch) {
    enum yarn_epoch_status old_status;
    yarn_word_t next_epoch = yarn_epoch_next(&old_status);
    
    fail_if(next_epoch != epoch, "next_epoch=%zu, expected=%zu", next_epoch, epoch);
    check_epoch_status(epoch, yarn_epoch_waiting);
    check_status(old_status, yarn_epoch_rollback);
    
    void* data = yarn_epoch_get_data(epoch);
    fail_if(data != VALUE, "data=%p, expected=%p", data, VALUE);
    
    void* task = (void*) yarn_epoch_get_task(epoch);
    fail_if(task != VALUE, "task=%p, expected=%p", task, VALUE);
  }

  for (int i = 0; i < 8; ++i) {
    enum yarn_epoch_status old_status;
    yarn_word_t next_epoch = yarn_epoch_next(&old_status);
    check_epoch_status(next_epoch, yarn_epoch_waiting);
    check_status(old_status, yarn_epoch_commit);
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

  {
    enum yarn_epoch_status old_status;
    yarn_word_t next_epoch = yarn_epoch_next(&old_status);

    bool ret = yarn_epoch_get_next_commit(&epoch_to_commit, &task, &data);
    fail_if(ret);

    yarn_epoch_set_executing(next_epoch);
    yarn_epoch_set_done(next_epoch);

    ret = yarn_epoch_get_next_commit(&epoch_to_commit, &task, &data);
    fail_if(!ret);
    yarn_epoch_set_commit(epoch_to_commit);
  }

  for (int j = 0; j < IT_COUNT; ++j) {

    for (int i = 0; i < IT_COUNT; ++i) {
      enum yarn_epoch_status old_status;
      yarn_word_t next_epoch = yarn_epoch_next(&old_status);
      yarn_epoch_set_task(next_epoch, (void*) VALUE);
      yarn_epoch_set_executing(next_epoch);
      yarn_epoch_set_done(next_epoch);

      check_epoch_status(next_epoch, yarn_epoch_done);
    }

    for (int i = 0; i < IT_COUNT; ++i) {
      bool ret = yarn_epoch_get_next_commit(&epoch_to_commit, &task, &data);

      fail_if(!ret);
      fail_if(task != (void*)VALUE, "task=%p, expected=%p", task, (void*) VALUE);
      check_epoch_status(epoch_to_commit, yarn_epoch_done);

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


// Simulates a basic epoch processing loop. Since the functions have to be used in a very
// specific way, we try to emulate actual use as much as possible.
//! \todo need to throw in some fail_*.
bool t_epoch_para_worker (yarn_word_t pool_id, void* task) {
  (void) pool_id;
  (void) task;

  for (int i = 0; i < 1000; ++i) {
    
    // printf("<%zu> - NEXT - START\n", pool_id);

    // Grab the next epoch to execute.
    enum yarn_epoch_status old_status;
    const yarn_word_t epoch = yarn_epoch_next(&old_status);

    // Rollback phase
    //   Rollback our epoch if necessary.
    {
      if (old_status == yarn_epoch_rollback) {
	// printf("<%zu> - NEXT=%zu -> ROLLBACK\n", pool_id, epoch);
	waste_time();
      }
      else {
	// printf("<%zu> - NEXT=%zu\n", pool_id, epoch);
      }
    }
    
    // Execution phase.
    //   Executes our epoch or randomly trigger rollbacks.
    {
      if (!yarn_epoch_set_executing(epoch)) {
	continue;
      }
      waste_time();
      if (rand() % 5 == 0) {
	// printf("<%zu> - ROLLBACK=%zu - START\n", pool_id, epoch+1);	  
	yarn_epoch_do_rollback(epoch+1);
	// printf("<%zu> - ROLLBACK=%zu - END\n", pool_id, epoch+1);	  
      }
      yarn_epoch_set_done(epoch);
    }

    // Commit phase
    //   Commit any finished epochs including our own.
    {
      yarn_word_t to_commit;
      void* task;
      void* data;
      while(yarn_epoch_get_next_commit(&to_commit, &task, &data)) {
	waste_time();
	// printf("<%zu> - COMMIT=%zu\n", pool_id, to_commit);
	yarn_epoch_set_commit(to_commit);
      }
    }
  }

  return true;
}

START_TEST(t_epoch_para) {
  yarn_tpool_init();
  // The more we run the better our chances to shake out a sync problem.
  for (int i = 0; i < 100; ++i) {
    yarn_tpool_exec(t_epoch_para_worker, NULL);
    
  }
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
