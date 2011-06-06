/*!
\author Rémi Attab
\license FreeBSD (see license file).
*/


#include "check_libyarn.h"

#include "t_utils.h"

#include <dependency.h>
#include <epoch.h>
#include <tpool.h>

#include <assert.h>
#include <stdio.h>


static yarn_word_t g_index_size;

static void t_dep_base_setup(void) {
  bool ret;
  
  ret = yarn_tpool_init();
  assert(ret);

  ret = yarn_epoch_init();
  assert(ret);

  g_index_size = 10;
  ret = yarn_dep_global_init(100, g_index_size);
  assert(ret);  
}

static void t_dep_base_teardown(void) {
  yarn_dep_global_destroy();
  yarn_epoch_destroy();
  yarn_tpool_destroy();
}


// fixture for sequential tests.
struct {
  yarn_word_t pid_1;
  yarn_word_t epoch_1;

  yarn_word_t pid_2;
  yarn_word_t epoch_2;

  yarn_word_t pid_3;
  yarn_word_t epoch_3;

  yarn_word_t pid_4;
  yarn_word_t epoch_4;

} f_seq;


static void t_dep_seq_setup(void) {
  t_dep_base_setup();

  assert(yarn_tpool_size() >= 4);

  enum yarn_epoch_status noop;

  f_seq.pid_1 = 0;
  f_seq.epoch_1 = yarn_epoch_next(&noop);
  yarn_dep_thread_init(f_seq.pid_1, f_seq.epoch_1);

  f_seq.pid_2 = 1;
  f_seq.epoch_2 = yarn_epoch_next(&noop);
  yarn_dep_thread_init(f_seq.pid_2, f_seq.epoch_2);

  f_seq.pid_3 = 2;
  f_seq.epoch_3 = yarn_epoch_next(&noop);
  yarn_dep_thread_init(f_seq.pid_3, f_seq.epoch_3);

  f_seq.pid_4 = 3;
  f_seq.epoch_4 = yarn_epoch_next(&noop);
  yarn_dep_thread_init(f_seq.pid_4, f_seq.epoch_4);

}

static void t_dep_seq_teardown(void) {

  yarn_dep_rollback(f_seq.epoch_1);
  yarn_dep_rollback(f_seq.epoch_2);
  yarn_dep_rollback(f_seq.epoch_3);
  yarn_dep_rollback(f_seq.epoch_4);

  yarn_dep_thread_destroy(f_seq.pid_1);
  yarn_dep_thread_destroy(f_seq.pid_2);
  yarn_dep_thread_destroy(f_seq.pid_3);
  yarn_dep_thread_destroy(f_seq.pid_4);

  t_dep_base_teardown();
}



START_TEST(t_dep_seq_load_store) {
  yarn_word_t mem = YARN_T_VALUE_1;

  t_yarn_check_dep_load(f_seq.pid_1, &mem, YARN_T_VALUE_1);
  t_yarn_check_dep_load(f_seq.pid_2, &mem, YARN_T_VALUE_1);
  t_yarn_check_dep_load(f_seq.pid_3, &mem, YARN_T_VALUE_1);

  t_yarn_check_dep_store(f_seq.pid_3, &mem, YARN_T_VALUE_2);

  t_yarn_check_epoch_status(f_seq.epoch_1, yarn_epoch_executing);
  t_yarn_check_epoch_status(f_seq.epoch_2, yarn_epoch_executing);
  t_yarn_check_epoch_status(f_seq.epoch_3, yarn_epoch_executing);
  t_yarn_check_epoch_status(f_seq.epoch_4, yarn_epoch_executing);

  t_yarn_check_dep_load(f_seq.pid_1, &mem, YARN_T_VALUE_1);
  t_yarn_check_dep_load(f_seq.pid_2, &mem, YARN_T_VALUE_1);
  t_yarn_check_dep_load(f_seq.pid_3, &mem, YARN_T_VALUE_2);
  t_yarn_check_dep_load(f_seq.pid_4, &mem, YARN_T_VALUE_2);

  t_yarn_check_dep_store(f_seq.pid_2, &mem, YARN_T_VALUE_1);

  t_yarn_check_epoch_status(f_seq.epoch_1, yarn_epoch_executing);
  t_yarn_check_epoch_status(f_seq.epoch_2, yarn_epoch_executing);
  t_yarn_check_epoch_status(f_seq.epoch_3, yarn_epoch_pending_rollback);
  t_yarn_check_epoch_status(f_seq.epoch_4, yarn_epoch_pending_rollback);

  t_yarn_check_dep_load(f_seq.pid_1, &mem, YARN_T_VALUE_1);
  t_yarn_check_dep_load(f_seq.pid_2, &mem, YARN_T_VALUE_1);

}
END_TEST

// A close mirror of seq_load_store. Should keep both in sync.
START_TEST(t_dep_seq_load_store_fast) {
  {
    yarn_word_t mem = YARN_T_VALUE_1;

    t_yarn_check_dep_load_fast(f_seq.pid_1, 0, &mem, YARN_T_VALUE_1);
    t_yarn_check_dep_load_fast(f_seq.pid_1, 0, &mem, YARN_T_VALUE_1);

    t_yarn_check_dep_load(f_seq.pid_1, &mem, YARN_T_VALUE_1);
    t_yarn_check_dep_store(f_seq.pid_1, &mem, YARN_T_VALUE_2);
 
    t_yarn_check_dep_load_fast(f_seq.pid_2, 0, &mem, YARN_T_VALUE_2);
    t_yarn_check_dep_load(f_seq.pid_2, &mem, YARN_T_VALUE_2);
  }

  {
    yarn_word_t mem = YARN_T_VALUE_3;
    
    t_yarn_check_dep_store_fast(f_seq.pid_1, 1, &mem, YARN_T_VALUE_4);
    t_yarn_check_dep_load_fast(f_seq.pid_1, 1, &mem, YARN_T_VALUE_4);

    t_yarn_check_dep_load(f_seq.pid_1, &mem, YARN_T_VALUE_4);
    t_yarn_check_dep_store(f_seq.pid_1, &mem, YARN_T_VALUE_1);
    
    t_yarn_check_dep_load_fast(f_seq.pid_2, 1, &mem, YARN_T_VALUE_1);
    t_yarn_check_dep_load(f_seq.pid_2, &mem, YARN_T_VALUE_1);
  }
}
END_TEST

START_TEST(t_dep_seq_commit) {
  yarn_word_t mem_1 = 0;
  yarn_word_t mem_2 = 0;
  yarn_word_t mem_3 = 0;
  yarn_word_t mem_4 = 0;

  t_yarn_check_dep_store(f_seq.pid_1, &mem_1, YARN_T_VALUE_1);
  t_yarn_check_dep_store(f_seq.pid_1, &mem_2, YARN_T_VALUE_1);

  t_yarn_check_dep_store(f_seq.pid_2, &mem_1, YARN_T_VALUE_2);
  t_yarn_check_dep_store(f_seq.pid_2, &mem_3, YARN_T_VALUE_2);

  t_yarn_check_dep_store(f_seq.pid_3, &mem_1, YARN_T_VALUE_3);
  t_yarn_check_dep_store(f_seq.pid_3, &mem_3, YARN_T_VALUE_3);
  t_yarn_check_dep_store(f_seq.pid_3, &mem_2, YARN_T_VALUE_3);
  t_yarn_check_dep_store(f_seq.pid_3, &mem_4, YARN_T_VALUE_3);

  t_yarn_check_dep_load(f_seq.pid_4, &mem_1, YARN_T_VALUE_3);
  t_yarn_check_dep_store(f_seq.pid_4, &mem_4, YARN_T_VALUE_4);
  t_yarn_check_dep_load(f_seq.pid_4, &mem_3, YARN_T_VALUE_3);
  t_yarn_check_dep_load(f_seq.pid_4, &mem_2, YARN_T_VALUE_3);


  yarn_dep_commit(f_seq.pid_2);
  t_yarn_check_dep_mem(f_seq.pid_2, mem_1, YARN_T_VALUE_2, "COMMIT");
  t_yarn_check_dep_mem(f_seq.pid_2, mem_2, 0, "COMMIT");
  t_yarn_check_dep_mem(f_seq.pid_2, mem_3, YARN_T_VALUE_2, "COMMIT");
  t_yarn_check_dep_mem(f_seq.pid_2, mem_4, 0, "COMMIT");
  
  yarn_dep_commit(f_seq.pid_1);
  t_yarn_check_dep_mem(f_seq.pid_1, mem_1, YARN_T_VALUE_2, "COMMIT");
  t_yarn_check_dep_mem(f_seq.pid_1, mem_2, YARN_T_VALUE_1, "COMMIT");
  t_yarn_check_dep_mem(f_seq.pid_1, mem_3, YARN_T_VALUE_2, "COMMIT");
  t_yarn_check_dep_mem(f_seq.pid_1, mem_4, 0, "COMMIT");

  yarn_dep_commit(f_seq.pid_4);
  t_yarn_check_dep_mem(f_seq.pid_4, mem_1, YARN_T_VALUE_2, "COMMIT");
  t_yarn_check_dep_mem(f_seq.pid_4, mem_2, YARN_T_VALUE_1, "COMMIT");
  t_yarn_check_dep_mem(f_seq.pid_4, mem_3, YARN_T_VALUE_2, "COMMIT");
  t_yarn_check_dep_mem(f_seq.pid_4, mem_4, YARN_T_VALUE_4, "COMMIT");

  yarn_dep_commit(f_seq.pid_3);
  t_yarn_check_dep_mem(f_seq.pid_3, mem_1, YARN_T_VALUE_3, "COMMIT");
  t_yarn_check_dep_mem(f_seq.pid_3, mem_2, YARN_T_VALUE_3, "COMMIT");
  t_yarn_check_dep_mem(f_seq.pid_3, mem_3, YARN_T_VALUE_3, "COMMIT");
  t_yarn_check_dep_mem(f_seq.pid_3, mem_4, YARN_T_VALUE_4, "COMMIT");

}
END_TEST

START_TEST(t_dep_seq_rollback) {  

  yarn_word_t mem = YARN_T_VALUE_1;

  t_yarn_check_dep_store(f_seq.pid_1, &mem, YARN_T_VALUE_2);
  t_yarn_check_dep_load(f_seq.pid_2, &mem, YARN_T_VALUE_2);
  t_yarn_check_dep_store(f_seq.pid_3, &mem, YARN_T_VALUE_3);
  t_yarn_check_dep_load(f_seq.pid_4, &mem, YARN_T_VALUE_3);
  
  yarn_epoch_do_rollback(f_seq.epoch_3);

  t_yarn_check_epoch_status(f_seq.epoch_1, yarn_epoch_executing);
  t_yarn_check_epoch_status(f_seq.epoch_2, yarn_epoch_executing);
  t_yarn_check_epoch_status(f_seq.epoch_3, yarn_epoch_pending_rollback);
  t_yarn_check_epoch_status(f_seq.epoch_4, yarn_epoch_pending_rollback);

  t_yarn_check_dep_load(f_seq.pid_1, &mem, YARN_T_VALUE_2);
  t_yarn_check_dep_load(f_seq.pid_2, &mem, YARN_T_VALUE_2);
  t_yarn_check_dep_load(f_seq.pid_3, &mem, YARN_T_VALUE_2);

  yarn_dep_rollback(f_seq.epoch_3);

  t_yarn_check_dep_load(f_seq.pid_1, &mem, YARN_T_VALUE_2);
  t_yarn_check_dep_load(f_seq.pid_2, &mem, YARN_T_VALUE_2);
  t_yarn_check_dep_load(f_seq.pid_3, &mem, YARN_T_VALUE_2);

  yarn_dep_rollback(f_seq.epoch_4);

  t_yarn_check_dep_store(f_seq.pid_3, &mem, YARN_T_VALUE_4);

  t_yarn_check_dep_load(f_seq.pid_1, &mem, YARN_T_VALUE_2);
  t_yarn_check_dep_load(f_seq.pid_2, &mem, YARN_T_VALUE_2);
  t_yarn_check_dep_load(f_seq.pid_3, &mem, YARN_T_VALUE_2);

  yarn_epoch_rollback_done(f_seq.epoch_3);
  yarn_epoch_rollback_done(f_seq.epoch_4);
  
  t_yarn_check_dep_load(f_seq.pid_1, &mem, YARN_T_VALUE_2);
  t_yarn_check_dep_load(f_seq.pid_2, &mem, YARN_T_VALUE_2);
  t_yarn_check_dep_load(f_seq.pid_3, &mem, YARN_T_VALUE_4);
  t_yarn_check_dep_load(f_seq.pid_4, &mem, YARN_T_VALUE_4);
  
}
END_TEST


static struct {
  yarn_word_t i;
  yarn_word_t acc;
  yarn_word_t n;
  yarn_word_t r;
} g_counter;


static void t_dep_para_setup (void) {
  t_dep_base_setup();

  g_counter.i = 0;
  g_counter.acc = 0;
  g_counter.n = 1000;
  g_counter.r = (g_counter.n*(g_counter.n+1))/2;
}
static void t_dep_para_teardown (void) {
  t_dep_base_teardown();
}


#define CHECK_DEP(x) if(!(x)) goto dep_error;
typedef enum {ok, done, err} ret_t;

ret_t t_dep_para_full_calc(yarn_word_t pool_id) {

  yarn_word_t i;
  CHECK_DEP(yarn_dep_load(pool_id, &g_counter.i, &i));
  i++;
  CHECK_DEP(yarn_dep_store(pool_id, &i, &g_counter.i));
      
  if (i > g_counter.n) {
    return done;
  }
      
  yarn_word_t acc;
  CHECK_DEP(yarn_dep_load(pool_id, &g_counter.acc, &acc));
  acc += i;
  CHECK_DEP(yarn_dep_store(pool_id, &acc, &g_counter.acc));

  return ok;

 dep_error:
  perror(__FUNCTION__);
  return err;
}

bool t_dep_para_full_worker(yarn_word_t pool_id, void* task) {
  (void) task;

  while(true) {
    enum yarn_epoch_status old_status;
    const yarn_word_t epoch = yarn_epoch_next(&old_status);

    if (old_status == yarn_epoch_rollback) {
      printf("\t\t\t\t\t\t\t\t<%zu> NEXT   => [%3zu] - ROLLBACK\n", pool_id, epoch);
      yarn_dep_rollback(epoch);
      yarn_epoch_rollback_done(epoch);
    }
    else {
      printf("\t\t\t\t\t\t\t\t<%zu> NEXT   => [%3zu]\n", pool_id, epoch);
    }

    bool init_ret = yarn_dep_thread_init(pool_id, epoch);
    if (!init_ret) goto init_error;
    
    ret_t calc_ret = t_dep_para_full_calc(pool_id);

    printf("\t\t\t\t\t\t\t\t<%zu> DONE   => [%3zu]\n", pool_id, epoch);
    yarn_epoch_set_done(epoch);
    yarn_dep_thread_destroy(pool_id);

    yarn_word_t commit_epoch;
    void* task;
    void* data;
    while (yarn_epoch_get_next_commit(&commit_epoch, &task, &data)) {
      printf("\t\t\t\t\t\t\t\t<%zu> COMMIT => [%3zu]\n", pool_id, commit_epoch);
      yarn_dep_commit(commit_epoch);
      yarn_epoch_commit_done(commit_epoch);
    }

    if (calc_ret == ok) {
      continue;
    }
    // This isn't quite right since some of the calc might end up being rolled-back.
    // It will do since we can't do it properly with just yarn_dep and yarn_epoch.
    else if (calc_ret == done) {
      printf("\t\t\t\t\t\t\t\t<%zu> DONE\n", pool_id);
      break;
    }
    else goto dep_error;

  }

  return true;

 dep_error:
 init_error:
  perror(__FUNCTION__);
  return false;

}

START_TEST(t_dep_para_full) {

  bool ret = yarn_tpool_exec(t_dep_para_full_worker, NULL);
  fail_if (!ret);
  fail_if (g_counter.acc != g_counter.r, 
	   "answer=%zu, expected=%zu", g_counter.acc, g_counter.r);
  
  yarn_word_t commit_epoch;
  void* task;
  void* data;    
  fail_if (yarn_epoch_get_next_commit(&commit_epoch, &task, &data));
}
END_TEST



void cycle_epoch(yarn_word_t epoch) {
  yarn_word_t commit_epoch;
  void* task;
  void* data;    

  yarn_epoch_set_done(epoch);
  yarn_epoch_get_next_commit(&commit_epoch, &task, &data);
  assert(epoch == commit_epoch);
  yarn_epoch_commit_done(commit_epoch);
}

START_TEST(t_dep_misc_edge) {
  const yarn_word_t pid_64 = 0;
  const yarn_word_t pid_65 = 1;

  enum yarn_epoch_status old_status;
  {  
    yarn_word_t epoch;
    do {
      epoch = yarn_epoch_next(&old_status);
      cycle_epoch(epoch);
    } while (epoch <= 49);
  }

  for (yarn_word_t i = 0; i < 16; ++i) {
    yarn_word_t epoch_64 = yarn_epoch_next(&old_status);
    yarn_dep_thread_init(pid_64, epoch_64);
    
    yarn_word_t epoch_65 = yarn_epoch_next(&old_status);
    yarn_dep_thread_init(pid_65, epoch_65);
    
    yarn_word_t mem = YARN_T_VALUE_1;
    t_yarn_check_dep_store(pid_64, &mem, YARN_T_VALUE_2);
    t_yarn_check_dep_load(pid_65, &mem, YARN_T_VALUE_2);
  }  

}
END_TEST



Suite* yarn_dep_suite (void) {
  Suite* s = suite_create("yarn_dep");

  TCase* tc_seq = tcase_create("yarn_dep.sequential");
  tcase_add_checked_fixture(tc_seq, t_dep_seq_setup, t_dep_seq_teardown);
  tcase_add_test(tc_seq, t_dep_seq_load_store);
  tcase_add_test(tc_seq, t_dep_seq_load_store_fast);
  tcase_add_test(tc_seq, t_dep_seq_commit);
  tcase_add_test(tc_seq, t_dep_seq_rollback);
  suite_add_tcase(s, tc_seq);

  TCase* tc_misc = tcase_create("yarn_dep.misc");
  tcase_add_checked_fixture(tc_misc, t_dep_base_setup, t_dep_base_teardown);
  tcase_add_test(tc_misc, t_dep_misc_edge);
  suite_add_tcase(s, tc_misc);

  TCase* tc_para = tcase_create("yarn_dep.parallel");
  tcase_add_checked_fixture(tc_para, t_dep_para_setup, t_dep_para_teardown);
  tcase_add_test(tc_para, t_dep_para_full);
  suite_add_tcase(s, tc_para);


  return s;
}
