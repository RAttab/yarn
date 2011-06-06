/*!
\author Rémi Attab
\license FreeBSD (see the LICENSE file).

Header used to supress debugging output.

\warning ONLY use in .c file and as the last include! Don't make this propagate 
through .h file.

To enable for a compilation unit, include like so:

#define YARN_DBG 1
#include "dbg.h"

To disable, either set YARN_DBG to 0 or don't define it.
To use the DBG macro, use it like this for a single line :

DBG printf("foobar=%d", 42);

for multiple lines:

DBG {
  int foobar = 42;
  printf("foobar=%d", foobar);
}

*/

#ifndef YARN_DBG
#  define YARN_DBG 0
#endif

#if YARN_DBG
#  define DBG if(1)
#else
#  define DBG if(0)
#endif

#undef YARN_DBG
