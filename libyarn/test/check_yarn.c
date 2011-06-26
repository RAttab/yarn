/*!
\author Rémi Attab
\license FreeBSD (see the LICENSE file)

The main yarn header for the target programs.
 */


#include "check_libyarn.h"

#include "t_utils.h"

#include <yarn.h>

#include <assert.h>
#include <stdio.h>

#define YARN_DBG 0
#include "dbg.h"


static void t_yarn_setup (void) {
  yarn_init();
}

static void t_yarn_teardown (void) {
  yarn_destroy();
}



typedef struct {
  yarn_word_t i;
  yarn_word_t acc;
  yarn_word_t n;
  yarn_word_t r;
} data_t;


#define CHECK_DEP(x) if(!(x)) goto dep_error;
#define INDEX_I 0
#define INDEX_ACC 1

enum yarn_ret t_yarn_exec_simple_worker (yarn_word_t pool_id, void* data) {
  data_t* counter = (data_t*) data;

  
  yarn_word_t i;
  CHECK_DEP(yarn_dep_load_fast(pool_id, INDEX_I, &counter->i, &i));
  i++;
  CHECK_DEP(yarn_dep_store_fast(pool_id, INDEX_I, &i, &counter->i));
      
  if (i > counter->n) {
    return yarn_ret_break;
  }
      
  yarn_word_t acc;
  CHECK_DEP(yarn_dep_load_fast(pool_id, INDEX_ACC, &counter->acc, &acc));
  acc += i;
  CHECK_DEP(yarn_dep_store_fast(pool_id, INDEX_ACC, &acc, &counter->acc));

  return yarn_ret_continue;

 dep_error:
  perror(__FUNCTION__);
  return yarn_ret_error;
  
}



START_TEST (t_yarn_exec_simple) {

  for (int i = 0; i < 100; ++i) {
    data_t counter;
    counter.i = 0;
    counter.acc = 0;
    counter.n = 1000;
    counter.r = (counter.n*(counter.n+1))/2;  

    bool ret = yarn_exec_simple(t_yarn_exec_simple_worker, &counter, 2, 2);

    fail_if (!ret);
    fail_if (counter.acc != counter.r, 
	     "answer=%zu, expected=%zu (i=%d)", counter.acc, counter.r, i);
    fail_if (counter.i != counter.n+1,
	     "i=%zu, expected=%zu", counter.i, counter.n+1);
  }
  
}
END_TEST


Suite* yarn_exec_suite (void) {
  Suite* s = suite_create("yarn_exec");

  TCase* tc_seq = tcase_create("yarn_exec");
  tcase_add_checked_fixture(tc_seq, t_yarn_setup, t_yarn_teardown);
  tcase_add_test(tc_seq, t_yarn_exec_simple);
  suite_add_tcase(s, tc_seq);

  return s;
}

