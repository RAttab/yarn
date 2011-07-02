/*!
\author Rémi Attab
\license FreeBSD (see license file).

*/


#include "check_libyarn.h"

#include <pstore.h>
#include <tpool.h>

#include <stdio.h>
#include <stdbool.h>


// Fixture
struct yarn_pstore* f_store;
static yarn_word_t f_pool_size;

static void t_pstore_setup(void) {
  bool ret = yarn_tpool_init();
  fail_if (!ret);

  f_store = yarn_pstore_init();
  fail_if(!f_store);

  f_pool_size = yarn_tpool_size();
  fail_if (f_pool_size <= 1, "Not enough CPUs detected (%d).", f_pool_size);
}

static void t_pstore_teardown(void) {
  yarn_pstore_destroy(f_store);
  yarn_tpool_destroy();
}


START_TEST(t_pstore_seq_init) {
  for (yarn_word_t pool_id = 0; pool_id < f_pool_size; ++pool_id) {
    
    void* val = yarn_pstore_load(f_store, pool_id);
    fail_if(val != NULL, "pool_id=%zu, val=%p, expected = %p", pool_id, val, NULL); 
  }

}
END_TEST

START_TEST(t_pstore_seq_load_store) {

  for (yarn_word_t pool_id = 0; pool_id < f_pool_size; ++pool_id) {
    yarn_pstore_store(f_store, pool_id, (void*) pool_id);

    yarn_word_t val = (yarn_word_t) yarn_pstore_load(f_store, pool_id);
    fail_if(val != pool_id, "pool_id=%zu, val=%zu, expected = %zu", pool_id, val, pool_id);
  }

  for (yarn_word_t pool_id = 0; pool_id < f_pool_size; ++pool_id) {
    yarn_word_t val = (yarn_word_t) yarn_pstore_load(f_store, pool_id);
    fail_if(val != pool_id, "pool_id=%zu, val=%zu, expected = %zu", pool_id, val, pool_id);
  }
}
END_TEST




bool t_pstore_worker (yarn_word_t pool_id, void* task) {

  (void) task;

  static const uintptr_t n = 1000;
  static const uintptr_t r = (n*(n+1))/2;


  for (uintptr_t i = 1; i <= 1000; ++i) {
    uintptr_t val = (uintptr_t)yarn_pstore_load(f_store, pool_id);
    val += i;
    yarn_pstore_store(f_store, pool_id, (void*) val);
  }

  uintptr_t val = (uintptr_t) yarn_pstore_load(f_store, pool_id);
  fail_if(val != r, "pool_id=%zu, val=%zu, expected = %zu", pool_id, val, r);

  return true;
}

START_TEST(t_pstore_para_load_store) {
  bool ret = yarn_tpool_exec(t_pstore_worker, NULL, YARN_TPOOL_ALL_THREADS);
  fail_if (!ret);
}
END_TEST



Suite* yarn_pstore_suite (void) {
  Suite* s = suite_create("yarn_pstore");

  TCase* tc_basic = tcase_create("yarn_pstore.basic");
  tcase_add_checked_fixture(tc_basic, t_pstore_setup, t_pstore_teardown);
  tcase_add_test(tc_basic, t_pstore_seq_init);
  tcase_add_test(tc_basic, t_pstore_seq_load_store);
  tcase_add_test(tc_basic, t_pstore_para_load_store);
  suite_add_tcase(s, tc_basic);

  return s;
}
