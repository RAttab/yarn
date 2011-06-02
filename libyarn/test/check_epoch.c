/*!
  \author Rémi Attab
  \license FreeBSD (see license file).
*/


#include "check_libyarn.h"

#include "t_utils.h"

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







START_TEST(t_epoch_next) {
  const yarn_word_t IT_COUNT = sizeof(yarn_word_t)*8/2;
  
  for (yarn_word_t i = 0; i < IT_COUNT; ++i) {
    enum yarn_epoch_status old_status;
    yarn_word_t next_epoch = yarn_epoch_next(&old_status);
    fail_if(next_epoch != i, "next_epoch=%zu, i=%zu", next_epoch, i);
    t_yarn_check_epoch_status(next_epoch, yarn_epoch_executing);
    t_yarn_check_status(old_status, yarn_epoch_commit);
  }
}
END_TEST


START_TEST(t_epoch_rollback_executing) {
  void* VALUE = (void*) 0xDEADBEEF;
  
  enum yarn_epoch_status old_status;
  yarn_word_t epoch = yarn_epoch_next(&old_status);
  t_yarn_set_epoch_data(epoch, VALUE);
  
  yarn_epoch_do_rollback(epoch);
  t_yarn_check_epoch_status(epoch, yarn_epoch_pending_rollback);
  t_yarn_check_epoch_data(epoch, VALUE);
  
  yarn_epoch_set_done(epoch);
  t_yarn_check_epoch_status(epoch, yarn_epoch_rollback);
  t_yarn_check_epoch_data(epoch, VALUE);
}
END_TEST

START_TEST(t_epoch_rollback_done) {
  void* VALUE = (void*) 0xDEADBEEF;

  enum yarn_epoch_status old_status;
  yarn_word_t epoch = yarn_epoch_next(&old_status);
  t_yarn_set_epoch_data(epoch, VALUE);
  
  yarn_epoch_set_done(epoch);
  t_yarn_check_epoch_status(epoch, yarn_epoch_done);
  t_yarn_check_epoch_data(epoch, VALUE);

  yarn_epoch_do_rollback(epoch);
  t_yarn_check_epoch_status(epoch, yarn_epoch_rollback);
  t_yarn_check_epoch_data(epoch, VALUE);
}
END_TEST

START_TEST(t_epoch_rollback_range) {
  void* VALUE = (void*) 0xDEADBEEF;
  
  for (int i = 0; i < 8; ++i) {
    enum yarn_epoch_status old_status;
    yarn_word_t epoch = yarn_epoch_next(&old_status);
    t_yarn_set_epoch_data(epoch, VALUE);
    yarn_epoch_set_done(epoch);
  }

  yarn_epoch_do_rollback(4);

  for (yarn_word_t epoch = 0; epoch < 4; ++epoch) {
    t_yarn_check_epoch_status(epoch, yarn_epoch_done);
    t_yarn_check_epoch_data(epoch, VALUE);
  }

  for (yarn_word_t epoch = 4; epoch < 8; ++epoch) {
    enum yarn_epoch_status old_status;
    yarn_word_t next_epoch = yarn_epoch_next(&old_status);
    
    fail_if(next_epoch != epoch, "next_epoch=%zu, expected=%zu", next_epoch, epoch);
    t_yarn_check_epoch_status(epoch, yarn_epoch_executing);
    t_yarn_check_status(old_status, yarn_epoch_rollback);
    t_yarn_check_epoch_data(epoch, VALUE);
  }

  for (int i = 0; i < 8; ++i) {
    enum yarn_epoch_status old_status;
    yarn_word_t next_epoch = yarn_epoch_next(&old_status);
    t_yarn_check_epoch_status(next_epoch, yarn_epoch_executing);
    t_yarn_check_status(old_status, yarn_epoch_commit);
  }

}
END_TEST

START_TEST(t_epoch_commit) {
  const int IT_COUNT = sizeof(yarn_word_t)*8/2;
  void* VALUE = (void*) 0xDEADBEEF;
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
    t_yarn_set_epoch_data(next_epoch, VALUE);

    bool ret = yarn_epoch_get_next_commit(&epoch_to_commit, &task, &data);
    fail_if(ret);

    yarn_epoch_set_done(next_epoch);

    ret = yarn_epoch_get_next_commit(&epoch_to_commit, &task, &data);
    fail_if(!ret);
    t_yarn_check_data(task, data, VALUE);
    yarn_epoch_commit_done(epoch_to_commit);
  }

  for (int j = 0; j < IT_COUNT; ++j) {

    for (int i = 0; i < IT_COUNT; ++i) {
      enum yarn_epoch_status old_status;
      yarn_word_t next_epoch = yarn_epoch_next(&old_status);
      t_yarn_set_epoch_data(next_epoch, VALUE);
      yarn_epoch_set_done(next_epoch);

      t_yarn_check_epoch_status(next_epoch, yarn_epoch_done);
      t_yarn_check_epoch_data(next_epoch, VALUE);
    }

    for (int i = 0; i < IT_COUNT; ++i) {
      bool ret = yarn_epoch_get_next_commit(&epoch_to_commit, &task, &data);

      fail_if(!ret);
      t_yarn_check_data(task, data, VALUE);
      t_yarn_check_epoch_status(epoch_to_commit, yarn_epoch_done);

      yarn_epoch_commit_done(epoch_to_commit);
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
	yarn_epoch_rollback_done(epoch);
      }
      else {
	// printf("<%zu> - NEXT=%zu\n", pool_id, epoch);
      }
    }
    
    // Execution phase.
    //   Executes our epoch or randomly trigger rollbacks.
    {
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
	yarn_epoch_commit_done(to_commit);
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
  tcase_add_test(tc_basic, t_epoch_rollback_executing);
  tcase_add_test(tc_basic, t_epoch_rollback_done);
  tcase_add_test(tc_basic, t_epoch_rollback_range);
  tcase_add_test(tc_basic, t_epoch_commit);
  tcase_add_test(tc_basic, t_epoch_para);
  suite_add_tcase(s, tc_basic);

  return s;
}
