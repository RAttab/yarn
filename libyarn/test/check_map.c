/*!
\author Rémi Attab
\license FreeBSD (see LICENSE file).

Tests for the concurrent hash map.
 */


#include "check_libyarn.h"

#include <map.h>

#include <stdint.h>
#include <stdio.h>


// Fixture.
struct yarn_map* f_map;


static void t_map_setup (void) {
  f_map = yarn_map_init(0);
}

static void t_map_teardown (void) {
  yarn_map_free(f_map);
  f_map = NULL;
}


//! \test Tests sequential adding and fetching a value on the yarn_map.
START_TEST(t_map_basic_add_and_get) {
  
  const int val_1 = 100;
  const uintptr_t addr_1 = (uintptr_t) &val_1;

  {
    void* r = yarn_map_probe(f_map, addr_1, (void*)&val_1);
    fail_unless(r == &val_1, "Failed simple add to yarn_map.");
    fail_unless(yarn_map_size(f_map) == 1, "Wrong map size after simple add.");
  }

  {
    void* r = yarn_map_probe(f_map, addr_1, (void*)&val_1);
    fail_unless(r == &val_1, "Failed simple get from yarn_map.");
    fail_unless(yarn_map_size(f_map) == 1, "Wrong map size after simple get.");
  }
}
END_TEST


//! \test Tests sequential duplicate keys and value adds to a yarn_map.
START_TEST(t_map_basic_add_duplicate) {
  
  const int val_1 = 100;
  const uintptr_t addr_1 = (uintptr_t) &val_1;

  const int val_2 = 200;
  const uintptr_t addr_2 = (uintptr_t) &val_2;

  yarn_map_probe(f_map, addr_1, (void*)&val_1);

  {  
    void* r = yarn_map_probe(f_map, addr_1, (void*)&val_2);
    fail_unless(r == &val_1, "Failed to add duplicate key to yarn_map.");
    fail_unless(yarn_map_size(f_map) == 1, "Wrong yarn_map size after duplicate key add.");
  }

  {
    void* r = yarn_map_probe(f_map, addr_2, (void*)&val_1);
    fail_unless(r == &val_1, "Failed to add duplicate value to yarn_map.");
    fail_unless(yarn_map_size(f_map) == 2, 
		"Wrong yarn_map size after duplicate value add.");
  }
}
END_TEST


/*!
\test Tests sequential resize on a yarn_map.
Keeps adding tons of items hoping to trigger a couple of resizes along the way.
*/
START_TEST (t_map_basic_resize) {
  
  int value = 100;

  for (size_t i = 1; i < 10000; ++i) {
    void* r = yarn_map_probe(f_map, i, (void*)&value);
    fail_unless (r == &value);
    fail_unless (yarn_map_size(f_map) == i, "Wrong map size aftr %d iterations.", i);
  }
}
END_TEST


Suite* yarn_map_suite (void) {
  Suite* s = suite_create("yarn_map");

  TCase* tc_basic = tcase_create("yarn_map.basic");
  tcase_add_checked_fixture(tc_basic, t_map_setup, t_map_teardown);
  tcase_add_test(tc_basic, t_map_basic_add_and_get);
  tcase_add_test(tc_basic, t_map_basic_add_duplicate);
  tcase_add_test(tc_basic, t_map_basic_resize);
  suite_add_tcase(s, tc_basic);

  /*
  TCase* tc_conc = tcase_create("yarn_map.concurent");
  tcase_unchecked_fixture(tc_conc, t_map_setup, t_map_teardown);
  tcase_add_test(t_conc, ...);
  suite_add_tcase(s, tc_conc);
  */

  return s;
}
