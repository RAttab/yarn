/*!
\author Rémi Attab
\license FreeBSD (see the LICENSE file).

 */

#ifndef YARN_TIMER_H_
#define YARN_TIMER_H_


#include "types.h"

#include <time.h>


// Nanosecond resolution of the timer.
typedef uint64_t yarn_time_t;

#define YARN_TIME_MAX UINT64_MAX



inline yarn_time_t yarn_time_from_timespec(struct timespec* ts) {
  yarn_time_t t = ts->tv_nsec;

  // In case of overflow the most significant bits should be trunked.
  //  This is the correct and expected behaviour.
  t += ts->tv_sec * 1000000000UL;

  return t;
}



inline yarn_time_t yarn_timer_sample_thread(void) {
  struct timespec ts;
  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);  
  return yarn_time_from_timespec(&ts);
}

inline yarn_time_t yarn_timer_sample_process(void) {
  struct timespec ts;
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
  return yarn_time_from_timespec(&ts);
}

inline yarn_time_t yarn_timer_sample_system(void) {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return yarn_time_from_timespec(&ts);
}



inline yarn_time_t yarn_timer_diff(yarn_time_t start, yarn_time_t end) {
  return start <= end ? end - start : YARN_TIME_MAX - end + start;
}

inline yarn_time_t yarn_timer_to_sec(yarn_time_t t) {
  return t / 1000000000UL;
}

inline yarn_time_t yarn_timer_to_msec(yarn_time_t t) {
  return t / 1000000UL;
}

inline yarn_time_t yarn_timer_to_usec(yarn_time_t t) {
  return t / 1000UL;
}

#endif // YARN_TIMER_H_
