/*!
\author Rémi Attab
\license FreeBSD (see license file).

*/


#include "check_libyarn.h"

#include <pmem.h>
#include <tpool.h>

#include <stdio.h>
#include <stdbool.h>


// Fixture
static struct yarn_pmem* f_good_mem;
static struct yarn_pmem* f_bad_mem;
static yarn_tsize_t f_pool_size;


struct t_struct {
  int value;
};


static bool t_struct_construct (void* data) {
  struct t_struct* s = (struct t_struct*) data;
  s->value = 0;
  return true;
}

static bool t_struct_fail_construct(void* data) {
  struct t_struct* s = (struct t_struct*) data;
  s->value = -1;
  return false;
}

static void t_struct_destruct (void* data) {
  struct t_struct* s = (struct t_struct*) data;
  s->value = -1;
}




static void t_pmem_setup(void) {
  bool ret = yarn_tpool_init();
  fail_if (!ret);

  f_good_mem = yarn_pmem_init(
      sizeof(struct t_struct), t_struct_construct, t_struct_destruct);
  fail_if(!f_good_mem);

  f_bad_mem = yarn_pmem_init(
      sizeof(struct t_struct), t_struct_fail_construct, t_struct_destruct);
  fail_if(!f_good_mem);

  f_pool_size = yarn_tpool_size();
  fail_if (f_pool_size <= 1, "Not enough CPUs detected (%d).", f_pool_size);

}

static void t_pmem_teardown(void) {
  yarn_pmem_destroy(f_good_mem);
  yarn_pmem_destroy(f_bad_mem);
  yarn_tpool_destroy();
}



START_TEST(t_pmem_fail_construct) {
  for (yarn_tsize_t pool_id = 0; pool_id < f_pool_size; ++pool_id) {
    struct t_struct* s = (struct t_struct*) yarn_pmem_alloc(f_bad_mem, pool_id);
    fail_if(s != NULL, "pool_id=%zu, val=%p, expected=%p", pool_id, s, NULL);
  }
}
END_TEST


START_TEST(t_pmem_free_null) {
  for (yarn_tsize_t pool_id = 0; pool_id < f_pool_size; ++pool_id) {
    yarn_pmem_free(f_bad_mem, pool_id, NULL);
  }
}
END_TEST


START_TEST(t_pmem_alloc) {
  for (yarn_tsize_t pool_id = 0; pool_id < f_pool_size; ++pool_id) {
    struct t_struct* s = NULL;

    {
      s = (struct t_struct*) yarn_pmem_alloc(f_good_mem, pool_id);
      fail_if(s == NULL, "pool_id=%zu, val=%p, expected=!NULL", pool_id, s);

      // Should place s in a reusable pool.
      yarn_pmem_free(f_good_mem, pool_id, s);
    }

    {
      // Should grab the freed value from the pool.
      struct t_struct* new_s = (struct t_struct*) yarn_pmem_alloc(f_good_mem, pool_id);
      fail_if(new_s != s, "pool_id=%zu, new_s=%p, expected=%p", pool_id, new_s, s);
      
      yarn_pmem_free(f_good_mem, pool_id, new_s);
    }

  }
}
END_TEST


START_TEST(t_pmem_force_free) {
  for(yarn_tsize_t pool_id = 0; pool_id < f_pool_size; ++pool_id) {
    struct t_struct* first_s = NULL;
    struct t_struct* second_s = NULL;

    {
      first_s = (struct t_struct*) yarn_pmem_alloc(f_good_mem, pool_id);
      second_s = (struct t_struct*) yarn_pmem_alloc(f_good_mem, pool_id);

      fail_if(first_s == second_s, "pool_id=%zu, first_s=%p, second_s=%p", 
	      pool_id, (void*)first_s, (void*)second_s);

      // Place in cache.
      yarn_pmem_free(f_good_mem, pool_id, first_s);

      // Should free redundant value.
      yarn_pmem_free(f_good_mem, pool_id, second_s);
    }


  }

}
END_TEST




Suite* yarn_pmem_suite (void) {
  Suite* s = suite_create("yarn_pmem");

  TCase* tc_basic = tcase_create("yarn_pmem.basic");
  tcase_add_checked_fixture(tc_basic, t_pmem_setup, t_pmem_teardown);
  tcase_add_test(tc_basic, t_pmem_fail_construct);
  tcase_add_test(tc_basic, t_pmem_free_null);
  tcase_add_test(tc_basic, t_pmem_alloc);
  tcase_add_test(tc_basic, t_pmem_force_free);
  suite_add_tcase(s, tc_basic);

  return s;
}
