/*!
\author Rémi Attab
\license FreeBSD (see LICENSE file).

Test launcher for libyarn.a
 */


#include "check_libyarn.h"

#include <check.h>
#include <stdlib.h>

#include <unistd.h>
#include <stdio.h>


int run_suite (Suite* s) {
  SRunner* sr = srunner_create(s);
  //srunner_set_fork_status(s, CK_FORK | CK_NOFORK);

  srunner_run_all(sr, CK_VERBOSE);
  
  int fail_count = srunner_ntests_failed(sr);
  srunner_free(sr);

  return fail_count;
}



int main (int argc, char** argv) {
  ((void) argc);
  ((void) argv);

  if (run_suite(yarn_map_suite()) > 0) return EXIT_FAILURE;
  
  return EXIT_SUCCESS;
}
