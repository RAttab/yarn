/*!
\author Rémi Attab
\license FreeBSD (see license file).

*/


#include "check_libyarn.h"

#include <pmem.h>
#include <tpool.h>

#include <stdio.h>
#include <stdbool.h>


struct t_struct {
  int value;
};

static bool t_struct_construct (void* data) {
  struct t_struct* s = (struct t_struct*) data;
  s->value = 0;
  return true;
}

static bool t_struct_fail_construct(void* data) {
  (void) data;
  return false;
}

static void t_struct_destruct (void* data) {
  struct t_struct* s = (struct t_struct*) data;
  (void) s;
}



// Fixture
struct yarn_pmem* f_mem;

static void t_pmem_setup(void) {
  bool ret = yarn_tpool_init();
  fail_if (!ret);

  f_mem = yarn_pmem_init(
      sizeof(struct t_struct),t_struct_construct, t_struct_destruct);
  fail_if(!f_mem);

}

static void t_pmem_teardown(void) {
  yarn_pmem_destroy(f_mem);
  yarn_tpool_destroy();
}


START_TEST(t_pmem_basic) {
  (void) t_struct_fail_construct(NULL);
}
END_TEST


Suite* yarn_pmem_suite (void) {
  Suite* s = suite_create("yarn_pmem");

  TCase* tc_basic = tcase_create("yarn_pmem.basic");
  tcase_add_checked_fixture(tc_basic, t_pmem_setup, t_pmem_teardown);
  tcase_add_test(tc_basic, t_pmem_basic);
  suite_add_tcase(s, tc_basic);

  return s;
}
