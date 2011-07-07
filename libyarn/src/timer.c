/*!
\author Rémi Attab
\license FreeBSD (see the LICENSE file).

 */


#include "yarn/timer.h"


extern inline yarn_time_t yarn_time_from_timespec(struct timespec* ts);

extern inline yarn_time_t yarn_timer_sample_thread(void);
extern inline yarn_time_t yarn_timer_sample_process(void);
extern inline yarn_time_t yarn_timer_sample_system(void);

extern inline yarn_time_t yarn_timer_diff(yarn_time_t start, yarn_time_t end);

extern inline yarn_time_t yarn_timer_to_sec(yarn_time_t t);
extern inline yarn_time_t yarn_timer_to_msec(yarn_time_t t);
extern inline yarn_time_t yarn_timer_to_usec(yarn_time_t t);


static yarn_time_t g_timer_dbg;


void yarn_timer_dbg_set() {
  g_timer_dbg = yarn_timer_sample_system();
}

yarn_time_t yarn_timer_dbg_get() {
  yarn_time_t sample = yarn_timer_sample_system();
  return yarn_timer_diff(g_timer_dbg, sample);
}

