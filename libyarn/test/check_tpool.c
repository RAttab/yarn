/*!
\author Rémi Attab
\license FreeBSD (see license file).

*/


#include "check_libyarn.h"

#include <yarn/types.h>
#include <atomic.h>
#include <tpool.h>

#include <stdio.h>
#include <stdlib.h>





// Fixture

struct t_worker {
  yarn_atomic_var good_count;
  yarn_atomic_var bad_count;
};

struct t_worker* f_worker;

static void t_tpool_setup(void) {
  bool ret = yarn_tpool_init();
  fail_if (!ret);

  const yarn_word_t thread_count = yarn_tpool_size();
  fail_if (thread_count <= 1, "Not enough CPUs detected (%d).", thread_count);

  f_worker = malloc(sizeof(struct t_worker));
  yarn_writev(&f_worker->bad_count, 0);
  yarn_writev(&f_worker->good_count, 0);
  fail_if (!f_worker);
}

static void t_tpool_teardown(void) {
  yarn_tpool_destroy();
  free(f_worker);
}


START_TEST(t_tpool_init) {
  // NOOP - Test an init followed by a destroy with nothing done in between.
  // Could fail since one of the barriers is lazy initialized.

  fail_if(false);
}
END_TEST



bool t_good_worker (yarn_word_t pool_id, void* task) {
  struct t_worker* w = (struct t_worker*) task;

  yarn_atomv_t old_count;
  yarn_atomv_t new_count;
  do {
    old_count = yarn_readv(&w->good_count);
    new_count = old_count + pool_id+1;
  } while (yarn_casv(&w->good_count, old_count, new_count) != old_count);

  // printf("pool_id=%zu -> GOOD -> old=%zu, new=%zu\n", pool_id, old_count, new_count);
  return true;
}

bool t_bad_worker (yarn_word_t pool_id, void* task) {
  if (pool_id+1 != yarn_tpool_size() /2) {
    return t_good_worker(pool_id, task);
  }
  else {
    struct t_worker* w = (struct t_worker*) task;

    yarn_atomv_t old_count;
    yarn_atomv_t new_count;
    do {
      old_count = yarn_readv(&w->bad_count);
      new_count = old_count + pool_id+1;
    } while (yarn_casv(&w->bad_count, old_count, new_count) != old_count);

    //printf("pool_id=%zu -> BAD -> old=%zu, new=%zu\n", pool_id, old_count, new_count);
    return false;
  }
}



static void exec_tpool(yarn_worker_t worker, 
		       yarn_word_t thread_count,
		       bool expected_ret, 
		       const yarn_word_t good_exp, 
		       const yarn_word_t bad_exp,
		       char* msg) 
{

  for (int i = 0; i < 2; ++i) {

    yarn_writev(&f_worker->good_count, 0);
    yarn_writev(&f_worker->bad_count, 0);

    bool ret = yarn_tpool_exec(worker, (void*)f_worker, thread_count);
    const yarn_atomv_t good_count = yarn_readv(&f_worker->good_count);
    const yarn_atomv_t bad_count = yarn_readv(&f_worker->bad_count);

    fail_if(ret != expected_ret,
	    "%s (%d) - ret=%d, expected=%d", msg, i, ret, expected_ret);

    fail_if(good_count != good_exp, 
	    "%s (%d) - good_count=%zu, expected=%zu", msg, i, good_count, good_exp);

    fail_if(bad_count != bad_exp,
	    "%s (%d) - bad_count=%zu, expected=%zu", msg, i, bad_count, bad_exp);
  }  
}

START_TEST(t_tpool_exec_good) {
  const yarn_word_t n = yarn_tpool_size();
  const yarn_word_t r = (n*(n+1))/2;

  exec_tpool(t_good_worker, YARN_TPOOL_ALL_THREADS, true, r, 0, "exec_good");

}
END_TEST

START_TEST(t_tpool_exec_bad) {
  const int n = yarn_tpool_size();
  const int r = (n*(n+1))/2;
  const int good_r = r - n/2;
  const int bad_r = n/2;

  exec_tpool(t_bad_worker, YARN_TPOOL_ALL_THREADS, false, good_r, bad_r, "first_bad");
  exec_tpool(t_good_worker, YARN_TPOOL_ALL_THREADS, true, r, 0, "second_good");
  exec_tpool(t_bad_worker, YARN_TPOOL_ALL_THREADS, false, good_r, bad_r, "third_bad");
  
}
END_TEST

START_TEST(t_tpool_exec_thread_count) {
  const yarn_word_t n = yarn_tpool_size();

  for (yarn_word_t i = 0; i <= n; ++i) {
    yarn_word_t j = i == 0 ? n : i;
    const yarn_word_t r = (j*(j+1))/2;

    exec_tpool(t_good_worker, i, true, r, 0, "exec_good");
  }


}
END_TEST


Suite* yarn_tpool_suite () {
  Suite* s = suite_create("yarn_tpool");

  TCase* tc_basic = tcase_create("yarn_tpool.basic");
  tcase_add_checked_fixture(tc_basic, t_tpool_setup, t_tpool_teardown);
  tcase_add_test(tc_basic, t_tpool_init);
  tcase_add_test(tc_basic, t_tpool_exec_good);
  tcase_add_test(tc_basic, t_tpool_exec_bad);
  tcase_add_test(tc_basic, t_tpool_exec_thread_count);
  suite_add_tcase(s, tc_basic);

  return s;
}
