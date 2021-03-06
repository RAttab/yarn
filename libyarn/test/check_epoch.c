/*!
  \author Rémi Attab
  \license FreeBSD (see license file).
*/


#include "check_libyarn.h"

#include "t_utils.h"

#include <yarn/types.h>
#include <epoch.h>
#include <tpool.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define YARN_DBG 0
#include "dbg.h"


static yarn_word_t g_epoch_max;

static void t_epoch_setup(void) {
  yarn_tpool_init();
  yarn_epoch_init();
  g_epoch_max = yarn_epoch_max();
}


static void t_epoch_teardown(void) {
  yarn_epoch_destroy();
  yarn_tpool_destroy();
}




START_TEST(t_epoch_next) {
  const yarn_word_t IT_COUNT = g_epoch_max / 2;
  
  for (yarn_word_t i = 0; i < IT_COUNT; ++i) {
    enum yarn_epoch_status old_status;
    yarn_word_t next_epoch;
    yarn_epoch_next(&next_epoch, &old_status);

    fail_if(next_epoch != i, "next_epoch=%zu, i=%zu", next_epoch, i);
    t_yarn_check_epoch_status(next_epoch, yarn_epoch_executing);
    t_yarn_check_status(old_status, yarn_epoch_commit);
  }
}
END_TEST


START_TEST(t_epoch_rollback_executing) {
  void* VALUE = (void*) YARN_T_VALUE_1;
  
  enum yarn_epoch_status old_status;
  yarn_word_t epoch;
  yarn_epoch_next(&epoch, &old_status);
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
  void* VALUE = (void*) YARN_T_VALUE_1;

  enum yarn_epoch_status old_status;
  yarn_word_t epoch;
  yarn_epoch_next(&epoch, &old_status);
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
  void* VALUE = (void*) YARN_T_VALUE_1;

  for (yarn_word_t i = 0; i < g_epoch_max/2; ++i) {
    enum yarn_epoch_status old_status;
    yarn_word_t epoch;
    yarn_epoch_next(&epoch, &old_status);
    t_yarn_set_epoch_data(epoch, VALUE);
    yarn_epoch_set_done(epoch);
  }

  const yarn_word_t rb_epoch = g_epoch_max/4;
  yarn_epoch_do_rollback(rb_epoch);

  for (yarn_word_t epoch = 0; epoch < rb_epoch; ++epoch) {
    t_yarn_check_epoch_status(epoch, yarn_epoch_done);
    t_yarn_check_epoch_data(epoch, VALUE);
  }

  for (yarn_word_t epoch = rb_epoch; epoch < g_epoch_max/2; ++epoch) {
    enum yarn_epoch_status old_status;
    yarn_word_t next_epoch;
    yarn_epoch_next(&next_epoch, &old_status);
    
    fail_if(next_epoch != epoch, "next_epoch=%zu, expected=%zu", next_epoch, epoch);
    t_yarn_check_epoch_status(epoch, yarn_epoch_executing);
    t_yarn_check_status(old_status, yarn_epoch_rollback);
    t_yarn_check_epoch_data(epoch, VALUE);
  }

  for (yarn_word_t i = 0; i < g_epoch_max/2; ++i) {
    enum yarn_epoch_status old_status;
    yarn_word_t next_epoch;
    yarn_epoch_next(&next_epoch, &old_status);
    t_yarn_check_epoch_status(next_epoch, yarn_epoch_executing);
    t_yarn_check_status(old_status, yarn_epoch_commit);
  }

}
END_TEST

START_TEST(t_epoch_commit) {
  const int IT_COUNT = g_epoch_max / 2;
  void* VALUE = (void*) YARN_T_VALUE_1;
  yarn_word_t epoch_to_commit;
  void* task;

  {
    bool ret = yarn_epoch_get_next_commit(&epoch_to_commit, &task);
    fail_if(ret);
  }

  {
    enum yarn_epoch_status old_status;
    yarn_word_t next_epoch;
    yarn_epoch_next(&next_epoch, &old_status);
    t_yarn_set_epoch_data(next_epoch, VALUE);

    bool ret = yarn_epoch_get_next_commit(&epoch_to_commit, &task);
    fail_if(ret);

    yarn_epoch_set_done(next_epoch);

    ret = yarn_epoch_get_next_commit(&epoch_to_commit, &task);
    fail_if(!ret);
    t_yarn_check_data(task, VALUE);
    yarn_epoch_commit_done(epoch_to_commit);
  }

  for (int j = 0; j < IT_COUNT; ++j) {

    for (int i = 0; i < IT_COUNT; ++i) {
      enum yarn_epoch_status old_status;
      yarn_word_t next_epoch;
      bool ret = yarn_epoch_next(&next_epoch, &old_status);
      fail_if(!ret);
      
      t_yarn_set_epoch_data(next_epoch, VALUE);
      yarn_epoch_set_done(next_epoch);

      t_yarn_check_epoch_status(next_epoch, yarn_epoch_done);
      t_yarn_check_epoch_data(next_epoch, VALUE);
    }

    for (int i = 0; i < IT_COUNT; ++i) {
      bool ret = yarn_epoch_get_next_commit(&epoch_to_commit, &task);

      fail_if(!ret);
      t_yarn_check_data(task, VALUE);
      t_yarn_check_epoch_status(epoch_to_commit, yarn_epoch_done);

      yarn_epoch_commit_done(epoch_to_commit);
    }

  }

  {
    bool ret = yarn_epoch_get_next_commit(&epoch_to_commit, &task);
    fail_if(ret);
  }

}
END_TEST

START_TEST(t_epoch_stop_basic) {
  enum yarn_epoch_status status;
  yarn_word_t epoch;

  for (yarn_word_t i = 0; i < g_epoch_max / 2; ++i) {
    bool ret = yarn_epoch_next(&epoch, &status);
    fail_if(!ret);
    yarn_epoch_set_done(epoch);
  }

  yarn_word_t stop_epoch = g_epoch_max / 4;
  yarn_epoch_stop((g_epoch_max / 2) - 1);
  yarn_epoch_stop(stop_epoch);

  // calling next here will block the thread so we can't test that.

  yarn_word_t to_commit;
  void* task;

  for (yarn_word_t i = 0; i <= stop_epoch; ++i) {

    bool ret = yarn_epoch_get_next_commit(&to_commit, &task);
    fail_if(!ret);
    yarn_epoch_commit_done(to_commit);
  }
  
  {
    bool ret = yarn_epoch_get_next_commit(&to_commit, &task);
    fail_if(ret);
  }

  {
    bool ret = yarn_epoch_next(&epoch, &status);
    fail_if(ret);
  }

}
END_TEST

START_TEST(t_epoch_stop_rollback_before) {
  yarn_word_t epoch;
  enum yarn_epoch_status status;
  
  const yarn_word_t stop_epoch = g_epoch_max / 2;

  for (yarn_word_t i = 0; i <= stop_epoch; ++i) {
    yarn_epoch_next(&epoch, &status);
    yarn_epoch_set_done(epoch);
  }

  yarn_epoch_stop(stop_epoch);

  const yarn_word_t rollback_epoch = g_epoch_max / 4;
  yarn_epoch_do_rollback(rollback_epoch);

  for (yarn_word_t i = rollback_epoch; i <= stop_epoch; ++i) {
    yarn_epoch_next(&epoch, &status);
    yarn_epoch_set_done(epoch);
  }

  
  for (yarn_word_t i = 0; i <= stop_epoch; ++i) {
    yarn_word_t to_commit;
    void* task;
    bool ret = yarn_epoch_get_next_commit(&to_commit, &task);
    fail_if(!ret);
    yarn_epoch_commit_done(to_commit);
  }

  {
    bool ret = yarn_epoch_next(&epoch, &status);
    fail_if(!ret);
  }
}
END_TEST


START_TEST(t_epoch_stop_rollback_after) {
  yarn_word_t epoch;
  enum yarn_epoch_status status;
  
  const yarn_word_t stop_epoch = g_epoch_max / 4;

  for (yarn_word_t i = 0; i <= g_epoch_max / 2; ++i) {
    yarn_epoch_next(&epoch, &status);
    yarn_epoch_set_done(epoch);
  }

  yarn_epoch_stop(stop_epoch);

  const yarn_word_t rollback_epoch = stop_epoch+1;
  yarn_epoch_do_rollback(rollback_epoch);

  yarn_word_t to_commit;
  void* task;
  
  for (yarn_word_t i = 0; i <= stop_epoch; ++i) {
    bool ret = yarn_epoch_get_next_commit(&to_commit, &task);
    fail_if(!ret);
    yarn_epoch_commit_done(to_commit);
  }

  {
    bool ret = yarn_epoch_get_next_commit(&to_commit, &task);
    fail_if(ret);
  }

  {
    bool ret = yarn_epoch_next(&epoch, &status);
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

  for (yarn_word_t i = 0; i < 2000; ++i) {
    
    DBG printf("<%zu> - NEXT - START\n", pool_id);

    // Grab the next epoch to execute.
    enum yarn_epoch_status old_status;
    yarn_word_t epoch;
    if(!yarn_epoch_next(&epoch, &old_status)) {
      break;
    }

    // Rollback phase
    //   Rollback our epoch if necessary.
    {
      if (old_status == yarn_epoch_rollback) {
	DBG printf("<%zu> - NEXT=%zu -> ROLLBACK\n", pool_id, epoch);
	waste_time();
	yarn_epoch_rollback_done(epoch);
      }
      else {
	DBG printf("<%zu> - NEXT=%zu\n", pool_id, epoch);
      }
    }
    
    // Execution phase.
    //   Executes our epoch or randomly trigger rollbacks.
    {
      waste_time();
      if (rand() % 5 == 0) {
	DBG printf("<%zu> - ROLLBACK=%zu - START\n", pool_id, epoch+1);	  
	yarn_epoch_do_rollback(epoch+1);
	DBG printf("<%zu> - ROLLBACK=%zu - END\n", pool_id, epoch+1);	  
      }
      
      if (i == 1000 + pool_id) {
	DBG printf("[%zu] - STOP\n", epoch);
	yarn_epoch_stop(epoch);
      }

      yarn_epoch_set_done(epoch);
    }

    // Commit phase
    //   Commit any finished epochs including our own.
    {
      yarn_word_t to_commit;
      void* task;
      while(yarn_epoch_get_next_commit(&to_commit, &task)) {
	waste_time();
	DBG printf("<%zu> - COMMIT=%zu\n", pool_id, to_commit);
	yarn_epoch_commit_done(to_commit);
      }
    }
  }

  return true;
}

START_TEST(t_epoch_para) {
  yarn_tpool_init();
  yarn_tpool_exec(t_epoch_para_worker, NULL, YARN_TPOOL_ALL_THREADS);
  yarn_tpool_destroy();
}
END_TEST



Suite* yarn_epoch_suite (bool para_only) {
  Suite* s = suite_create("yarn_epoch");

  if (!para_only) {
    TCase* tc_basic = tcase_create("yarn_epoch.seq");
    tcase_add_checked_fixture(tc_basic, t_epoch_setup, t_epoch_teardown);
    tcase_add_test(tc_basic, t_epoch_next);
    tcase_add_test(tc_basic, t_epoch_rollback_executing);
    tcase_add_test(tc_basic, t_epoch_rollback_done);
    tcase_add_test(tc_basic, t_epoch_rollback_range);
    tcase_add_test(tc_basic, t_epoch_commit);
    tcase_add_test(tc_basic, t_epoch_stop_basic);
    tcase_add_test(tc_basic, t_epoch_stop_rollback_before);
    tcase_add_test(tc_basic, t_epoch_stop_rollback_after);
    suite_add_tcase(s, tc_basic);
  }

  TCase* tc_para = tcase_create("yarn_epoch.para");
  tcase_add_checked_fixture(tc_para, t_epoch_setup, t_epoch_teardown);
  tcase_add_test(tc_para, t_epoch_para);
  suite_add_tcase(s, tc_para);

  return s;
}
