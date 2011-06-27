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
#include <stdbool.h>


int run_suite (Suite* s) {
  SRunner* sr = srunner_create(s);
  //srunner_set_fork_status(sr, CK_NOFORK);

  srunner_run_all(sr, CK_VERBOSE);
  
  int fail_count = srunner_ntests_failed(sr);
  srunner_free(sr);

  return fail_count;
}


#include <helper.h>

int main (int argc, char** argv) {
  ((void) argc);
  ((void) argv);

  bool para_only = argc > 1;

  bool err = false;

  err |= run_suite(yarn_map_suite(para_only)) > 0;  
  if (!para_only) err |= run_suite(yarn_bits_suite()) > 0;

  if (!para_only) err |= run_suite(yarn_tpool_suite()) > 0; 
  if (!para_only) err |= run_suite(yarn_pstore_suite()) > 0;
  if (!para_only) err |= run_suite(yarn_pmem_suite()) > 0;

  err |= run_suite(yarn_epoch_suite(para_only)) > 0;
  err |= run_suite(yarn_dep_suite(para_only)) > 0;
  err |= run_suite(yarn_exec_suite(para_only)) > 0;

  
  return err ? EXIT_FAILURE : EXIT_SUCCESS;
}
