/*!
\author Rémi Attab
\license FreeBSD (see license file).

*/


#include "check_libyarn.h"

#include <atomic.h>
#include <tpool.h>
#include <alloc.h>

#include <stdio.h>
#include <stdbool.h>




// Fixture

struct t_worker {
  yarn_atomic_var good_count;
  yarn_atomic_var bad_count;
};

struct t_worker* f_worker;

static void t_tpool_setup(void) {
  bool ret = yarn_tpool_init();
  fail_if (!ret);

  const yarn_tsize_t thread_count = yarn_tpool_size();
  fail_if (thread_count <= 1, "Not enough CPUs detected (%d).", thread_count);

  f_worker = yarn_malloc(sizeof(struct t_worker));
  yarn_writev(&f_worker->bad_count, 0);
  yarn_writev(&f_worker->good_count, 0);
  fail_if (!f_worker);
}

static void t_tpool_teardown(void) {
  yarn_tpool_destroy();
  yarn_free(f_worker);
}


START_TEST(t_tpool_init) {
  // NOOP - Test an init followed by a destroy with nothing done in between.
  // Could fail since one of the barriers is lazy initialized.

  fail_if(false);
}
END_TEST



bool t_good_worker (yarn_tsize_t pool_id, void* task) {
  struct t_worker* w = (struct t_worker*) task;

  yarn_atomv_t old_count;
  yarn_atomv_t new_count;
  do {
    old_count = yarn_readv(&w->good_count);
    new_count = old_count + pool_id+1;
  } while (yarn_casv(&w->good_count, old_count, new_count) != old_count);

  //printf("pool_id=%zu -> GOOD -> old=%zu, new=%zu\n", pool_id, old_count, new_count);
  return true;
}

bool t_bad_worker (yarn_tsize_t pool_id, void* task) {
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
		       bool expected_ret, 
		       const yarn_tsize_t good_exp, 
		       const yarn_tsize_t bad_exp,
		       char* msg) 
{

  for (int i = 0; i < 2; ++i) {

    yarn_writev(&f_worker->good_count, 0);
    yarn_writev(&f_worker->bad_count, 0);

    bool ret = yarn_tpool_exec(worker, (void*)f_worker);
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
  const yarn_tsize_t n = yarn_tpool_size();
  const yarn_tsize_t r = (n*(n+1))/2;

  exec_tpool(t_good_worker, true, r, 0, "exec_good");

}
END_TEST

START_TEST(t_tpool_exec_bad) {
  const int n = yarn_tpool_size();
  const int r = (n*(n+1))/2;
  const int good_r = r - n/2;
  const int bad_r = n/2;

  exec_tpool(t_bad_worker, false, good_r, bad_r, "first_bad");
  exec_tpool(t_good_worker, true, r, 0, "second_good");
  exec_tpool(t_bad_worker, false, good_r, bad_r, "third_bad");
  
}
END_TEST

Suite* yarn_tpool_suite (void) {
  Suite* s = suite_create("yarn_tpool");

  TCase* tc_basic = tcase_create("yarn_tpool.basic");
  tcase_add_checked_fixture(tc_basic, t_tpool_setup, t_tpool_teardown);
  tcase_add_test(tc_basic, t_tpool_init);
  tcase_add_test(tc_basic, t_tpool_exec_good);
  tcase_add_test(tc_basic, t_tpool_exec_bad);
  suite_add_tcase(s, tc_basic);

  return s;
}
