/*!
\author Rémi Attab
\license FreeBSD (see LICENSE file)
*/

#include "t_utils.h"

#include <time.h>


static struct timespec g_base;
static bool g_base_is_set = false;


void set_base_time() {
  clock_gettime(CLOCK_MONOTONIC, &g_base);  
  g_base_is_set = true;
}


long get_rel_time() {
  assert(g_base_is_set);

  struct timespec cur;
  clock_gettime(CLOCK_MONOTONIC, &cur);
  return (cur.tv_nsec - g_base.tv_nsec) / 100000;
}
