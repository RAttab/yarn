/*!
\author Rémi Attab
\license FreeBSD (see license file).

*/


#include "check_libyarn.h"

#include <tpool.h>

#include <stdio.h>
#include <stdbool.h>


// Fixture

static void t_tpool_setup(void) {
  bool ret = yarn_tpool_init();
  fail_if (!ret);
}

static void t_tpool_teardown(void) {
  yarn_tpool_destroy();
}


START_TEST(t_tpool_basic) {

}
END_TEST


Suite* yarn_tpool_suite (void) {
  Suite* s = suite_create("yarn_tpool");

  TCase* tc_basic = tcase_create("yarn_tpool.basic");
  tcase_add_checked_fixture(tc_basic, t_tpool_setup, t_tpool_teardown);
  tcase_add_test(tc_basic, t_tpool_basic);
  suite_add_tcase(s, tc_basic);

  return s;
}
