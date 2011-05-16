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

static void t_pstore_setup(void) {
  bool ret = yarn_tpool_init();
  fail_if (!ret);

  f_store = yarn_pstore_init();
  fail_if(!f_store);

}

static void t_pstore_teardown(void) {
  yarn_pstore_destroy(f_store);
  yarn_tpool_destroy();
}


START_TEST(t_pstore_basic) {

}
END_TEST


Suite* yarn_pstore_suite (void) {
  Suite* s = suite_create("yarn_pstore");

  TCase* tc_basic = tcase_create("yarn_pstore.basic");
  tcase_add_checked_fixture(tc_basic, t_pstore_setup, t_pstore_teardown);
  tcase_add_test(tc_basic, t_pstore_basic);
  suite_add_tcase(s, tc_basic);

  return s;
}
