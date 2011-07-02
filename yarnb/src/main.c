/*!
\author Rémi Attab
\license FreeBSD (see the LICENSE file).
 */

#include <yarn.h>
#include <yarn/timer.h>

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>


struct task {
  size_t i;
  size_t n;
  
  yarn_time_t wait_time;

  size_t array_size;
  yarn_word_t array[];

};

#define DEFAULT_ARRAY_SIZE 16
#define DEFAULT_N (DEFAULT_ARRAY_SIZE * DEFAULT_ARRAY_SIZE)

#define TIME_END_NS 1000000
#define TIME_STEP_NS 100

#define SPEEDUP_MIN 0.5
#define SPEEDUP_STEP 0.5

#define DEBUG "DEBUG - "
#define INFO  "INFO  - "
#define WARN  "WARN  - "
#define ERROR "ERROR - "

double get_speedup(yarn_time_t wait_time);

int speedup_search (const double target_speedup,
		     const yarn_time_t start_time, 
		     const yarn_time_t end_time, 
		     yarn_time_t* speedup_time);
yarn_time_t speedup_lower_bound (const double start_speedup, 
				 const yarn_time_t start_time, 
				 const yarn_time_t step);

void run_normal (struct task* t);
enum yarn_ret run_speculative (yarn_word_t pool_id, void* task);



static double g_max_speedup;
static double g_min_speedup;


inline int comp_speedup (double a, double b) {
  static const double epsilon = 0.1;

  if (fabs(a - b) < epsilon) {
    return 0;
  }

  return a > b ? 1 : -1;
}
inline int comp_time (yarn_time_t a, yarn_time_t b) {
  static const yarn_time_t epsilon = 100;

  if (llabs(a - b) < epsilon) {
    return 0;
  }

  return a > b ? 1 : -1;
}



int main (int argc, char** argv) {
  (void) argc;
  (void) argv;

  bool ret = yarn_init();
  if (!ret) goto yarn_error;

  // Get from command line?
  yarn_time_t start_time = 0;
  const yarn_time_t end_time = TIME_END_NS;
  const yarn_time_t time_step = TIME_STEP_NS;

  const double min_speedup = SPEEDUP_MIN;
  const double max_speedup = yarn_thread_count();
  const double speedup_step = SPEEDUP_STEP;

  printf(INFO "Yarn benchmark tests.\n");
  printf(INFO "\tSpeculative threads = %zu\n", yarn_thread_count());

  printf(INFO "\tSpeedup range = [%f, %f]\n", min_speedup, max_speedup);
  printf(INFO "\tSpeedup delta = %f\n", speedup_step);

  printf(INFO "\tTime range = [0ns, %zuns]\n", end_time);
  printf(INFO "\tTime delta = %zuns\n", time_step);


  printf(INFO "Warming up...\n");
  for (int i = 0; i < 10; ++i) {
    (void) get_speedup(0);
  }

  printf(INFO "Getting bounds...\n");
  g_max_speedup = get_speedup(end_time);
  g_min_speedup = get_speedup(start_time);
  printf(INFO "\tMIN_TIME=%zu => SPEEDUP=%f\n", start_time, g_min_speedup);
  printf(INFO "\tMAX_TIME=%zu => SPEEDUP=%f\n", end_time, g_max_speedup);

  printf(INFO "Executing the benchmark...\n");
  double best_speedup = 0.0;
  for (double speedup = min_speedup; speedup <= max_speedup; speedup += speedup_step) {

    yarn_time_t speedup_time;
    int ret = speedup_search(speedup, start_time, end_time, &speedup_time);
    if (ret > 0) {
      break;
    }
    else if (ret < 0) {
      printf(INFO "SPEEDUP=%f => SKIPPED (too small)\n", speedup);
      best_speedup = speedup;
      continue;
    }

    //    speedup_time = speedup_lower_bound(speedup, start_time, time_step);
    start_time = speedup_time;

    printf(INFO "SPEEDUP=%f => TIME=%zuns\n", speedup, speedup_time);
    best_speedup = speedup;
  }


  if (comp_speedup(best_speedup, min_speedup) >= 0) {
    printf(INFO "Best speedup achieved => %f\n", best_speedup);
  }
  else {
    printf(WARN "Unable to achieve minimal speedup => %f\n", min_speedup);
  }


  yarn_destroy();
  return 0;

  yarn_destroy();
 yarn_error:
  perror(__FUNCTION__);
  return 1;
}

int speedup_search (const double target_speedup,
		     yarn_time_t start_time, 
		     yarn_time_t end_time, 
		     yarn_time_t* speedup_time) 
{
  printf(DEBUG "searching for => %f\n", target_speedup);

  double start_speedup = g_min_speedup;
  if (comp_speedup(start_speedup, target_speedup) > 0) {
    printf(WARN "Target is below start_time (start_speedup=%f)\n", start_speedup);
    return -1;
  }

  double end_speedup = g_max_speedup;
  if (comp_speedup(end_speedup, target_speedup) < 0) {
    printf(WARN "Target is above end_time (end_speedup=%f)\n", end_speedup);
    return 1;
  }

  
  // interpolation search.
  double speedup;
  yarn_time_t time;
  int speedup_comp;
  do {

    double slope = 
      ((double) end_speedup - start_speedup) / 
      ((double) end_time - start_time);

    time = (target_speedup - start_speedup) / slope;

    if (time > TIME_END_NS) {
      time = TIME_END_NS;
    }

    printf(DEBUG "search - time=%zu, slope=%f (speedup=[%f, %f], time=[%zu, %zu])\n", 
	   time, slope, start_speedup, end_speedup, start_time, end_time); 

    speedup = get_speedup(time);
    
    speedup_comp = comp_speedup(speedup, target_speedup);
    if (speedup_comp < 0) {
      start_time = time;
      start_speedup = speedup;
    }
    else if (speedup_comp > 0) {
      end_time = time;
      end_speedup = speedup;
    }

  } while (speedup_comp != 0);

  printf(DEBUG "Target time => %zuns\n", time);

  *speedup_time = time;
  return 0;
}

yarn_time_t speedup_lower_bound (const double start_speedup, 
				 const yarn_time_t start_time, 
				 const yarn_time_t step) 
{
  printf(DEBUG "lower_bound for speedup => %f\n", start_speedup);

  double speedup;
  yarn_time_t prev_time;
  yarn_time_t time = start_time;

  do {
    prev_time = time;
    time -= step;    

    if (prev_time < step) {
      break;
    }

    printf(DEBUG "lower_bound it=%zu\n", time);

    speedup = get_speedup(time);
    
  } while (comp_speedup(speedup, start_speedup) == 0);

  printf(DEBUG "lower_bound time => %zuns\n", prev_time);

  return prev_time;
}


struct task* create_task (size_t array_size, size_t n, yarn_time_t wait_time) {
  size_t t_size = sizeof(struct task) + sizeof(yarn_word_t)*array_size;
  struct task* t = (struct task*) malloc(t_size);
  if (!t) goto alloc_error;

  t->i = 0;
  t->n = n;
  t->wait_time = wait_time;
  t->array_size = array_size;
  for (size_t i = 0; i < array_size; ++i) {
    t->array[i] = 0;
  }

  return t;
 
 alloc_error:
  perror(__FUNCTION__);
  return NULL;
}


typedef void (*exec_func_t) (struct task*);


void exec_normal (struct task* t) {
  run_normal(t);
}

void exec_speculative (struct task* t) {
  bool ret = yarn_exec_simple(run_speculative, (void*) t, 
			      YARN_ALL_THREADS, t->array_size, 0);
  assert(ret);
}


yarn_time_t time_exec (exec_func_t exec_func, yarn_time_t wait_time) {
  static const int n = 10;

  yarn_time_t time_sum = 0;
  yarn_time_t min_time = UINT64_MAX;
  yarn_time_t max_time = 0;

  for (int i = 0; i < n; ++i) {
    struct task* t = create_task(DEFAULT_ARRAY_SIZE, DEFAULT_N, wait_time);
    yarn_time_t start = yarn_timer_sample_system();

    (*exec_func)(t);

    yarn_time_t end = yarn_timer_sample_system();
    yarn_time_t diff_time = yarn_timer_diff(start, end);

    time_sum += diff_time;
    if (diff_time < min_time) {
      min_time = diff_time;
    }
    if (diff_time > max_time) {
      max_time = diff_time;
    }

    free(t);
  }

  time_sum -= min_time;
  time_sum -= max_time;

  return time_sum / (n-2);  
}



// Could rerun each a couple of time to make sure that we're within our epsilon.
double get_speedup(yarn_time_t wait_time) {
  assert(wait_time <= TIME_END_NS);

  yarn_time_t base_time = time_exec(exec_normal, wait_time);
  yarn_time_t speculative_time = time_exec(exec_speculative, wait_time);

  double speedup = (double) base_time / (double) speculative_time;

  printf(DEBUG "time=%zu => speedup=%f (base=%zu, spec=%zu)\n", 
	 wait_time, speedup, base_time, speculative_time);

  return speedup;
}




void look_busy (yarn_word_t* value, yarn_time_t wait_time) {
  yarn_time_t start = yarn_timer_sample_thread();
  yarn_time_t elapsed;
  
  do {
    yarn_time_t sample = yarn_timer_sample_thread();
    elapsed = yarn_timer_diff(start, sample);
  } while (elapsed <= wait_time);

  *value = elapsed;
}


void run_normal (struct task* t) {
  for (size_t i = 0; i < t->n; ++i) {
    size_t src = i % t->array_size;
    size_t dest = (src+1) % t->array_size;
    
    yarn_word_t value = t->array[src];
    look_busy(&value, t->wait_time);
    t->array[dest] = value;
  }
}

enum yarn_ret run_speculative (yarn_word_t pool_id, void* task) {
  struct task* t = (struct task*) task;

  size_t i;
  yarn_dep_load(pool_id, &t->i, &i);
  i++;
  yarn_dep_store(pool_id, &i, &t->i);

  if (i >= t->n) {
    return yarn_ret_break;
  }

  size_t src = i % t->array_size;
  //  size_t dest = (src+1) % t->array_size;    
  size_t dest = src;
  yarn_word_t value;

  yarn_dep_load(pool_id, &t->array[src], &value);
  look_busy(&value, t->wait_time);  
  yarn_dep_store(pool_id, &value, &t->array[dest]);

  return yarn_ret_continue;
}

