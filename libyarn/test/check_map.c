/*!
\author Rémi Attab
\license FreeBSD (see LICENSE file).

Tests for the concurrent hash map.
 */


#include "check_libyarn.h"

#include <map.h>

#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <sched.h>


// Fixture.
struct yarn_map* f_map;

static void t_map_basic_setup (void) {
  f_map = yarn_map_init(0);
}

static void t_map_basic_teardown (void) {
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


static int NUM_CPU;

typedef void* (*worker_func_t) (void*);
enum tid_t {TID_1 = 1, TID_2 = 2};
pthread_t thread_1, thread_2;
pthread_barrier_t start_barrier, extra_barrier;

static void t_map_para_setup (void) {
  pthread_barrier_init(&start_barrier, NULL, 2);
  pthread_barrier_init(&extra_barrier, NULL, 2);
  t_map_basic_setup();
}
static void t_map_para_teardown (void) {
  t_map_basic_teardown();
  pthread_barrier_destroy(&extra_barrier);
  pthread_barrier_destroy(&start_barrier);
}


static void t_map_para_launch (pthread_t* thread, 
			       worker_func_t worker, 
			       enum tid_t* tid, 
			       int affinity) 
{
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(affinity, &cpuset);
  
  int ret = pthread_create(thread, NULL, worker, tid);
  if (ret != 0) {
    perror("t_map_para_launch");
    fail("Failed to start the test threads");
    return;
  }

  pthread_setaffinity_np(*thread, sizeof(cpu_set_t), &cpuset);
}

#define PARA_ADD_COUNT 10000
#define WORKER_START(tid_name) \
  const enum tid_t tid_name = *((enum tid_t*)param); \
  pthread_barrier_wait(&start_barrier);


static void* t_map_para_distinct_add_worker (void* param) {
  WORKER_START(tid);

  const int val = 100 * tid;

  uintptr_t addr_start = 100000 * tid;
  uintptr_t addr_end = addr_start + PARA_ADD_COUNT;

  for (uintptr_t addr = addr_start; addr < addr_end; ++addr) {
    void* ret = yarn_map_probe(f_map, addr, (void*)&val);
    fail_unless(ret == &val,
		"Failed to concurrently add a single distinct element concurrently.");
  }

  return NULL;
}

/*!
Adds a multiple elements with no key conflicts to a yarn_map.
 */
START_TEST (t_map_para_distinct_add) {
  enum tid_t tid_1 = TID_1, tid_2 = TID_2;
  t_map_para_launch(&thread_1, t_map_para_distinct_add_worker, &tid_1, 0);
  t_map_para_launch(&thread_2, t_map_para_distinct_add_worker, &tid_2, 1);

  pthread_join(thread_1, NULL);
  pthread_join(thread_2, NULL);

  fail_unless(yarn_map_size(f_map) == PARA_ADD_COUNT *2,
	      "Failed to properly add distinct elements concurrently to yarn_map.");
}
END_TEST





static void* t_map_para_duplicate_add_worker (void* param) {
  WORKER_START(tid);

  const int val = 100 * tid;
  const int ele_count = 20;

  for (size_t n = 0; n < PARA_ADD_COUNT; n++) {
    uintptr_t addr = (n % ele_count);
    if (tid == TID_2)
      addr = ele_count - addr;

    addr += 1;

    yarn_map_probe(f_map, addr, (void*)&val);
  }
  
  return NULL;
}

START_TEST (t_map_para_duplicate_add) {
  enum tid_t tid_1 = TID_1, tid_2 = TID_2;
  t_map_para_launch(&thread_1, t_map_para_duplicate_add_worker, &tid_1, 0);
  t_map_para_launch(&thread_2, t_map_para_duplicate_add_worker, &tid_2, 1);

  pthread_join(thread_1, NULL);
  pthread_join(thread_2, NULL);

  fail_unless(yarn_map_size(f_map) == 20,
	      "Failed to properly add duplicate elements concurrently to yarn_map.");
}
END_TEST


Suite* yarn_map_suite (void) {
  Suite* s = suite_create("yarn_map");

  TCase* tc_basic = tcase_create("yarn_map.basic");
  tcase_add_checked_fixture(tc_basic, t_map_basic_setup, t_map_basic_teardown);
  tcase_add_test(tc_basic, t_map_basic_add_and_get);
  tcase_add_test(tc_basic, t_map_basic_add_duplicate);
  tcase_add_test(tc_basic, t_map_basic_resize);
  suite_add_tcase(s, tc_basic);


  NUM_CPU = sysconf(_SC_NPROCESSORS_ONLN);
  if (NUM_CPU > 1) {
    printf("Detected %d CPUs. Running parallel tests.\n", NUM_CPU);

    TCase* tc_conc = tcase_create("yarn_map.concurent");
    tcase_add_unchecked_fixture(tc_conc, t_map_para_setup, t_map_para_teardown);
    tcase_add_test(tc_conc, t_map_para_distinct_add);
    tcase_add_test(tc_conc, t_map_para_duplicate_add);
    suite_add_tcase(s, tc_conc);
  }
  else {
    printf("Detected only a single CPU. Skipping parallel tests.\n");
  }

  return s;
}
