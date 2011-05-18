/*!
\author Rémi Attab
\license FreeBSD (see license file).

Several helper macros, constants and whatever else I consider helperful.

*/

#ifndef YARN_HELPER_H_
#define YARN_HELPER_H_

#include <stdbool.h>
#include <assert.h>

#define YARN_CHECK_ERR()  \
  do {		  \
    perror(__FUNCTION__); \
    assert(false);	  \
  } while(0)


#ifndef NDEBUG

#define YARN_CHECK_RET0(expr) \
  do { if((expr) != 0) { YARN_CHECK_ERR(); } } while(0)
#define YARN_CHECK_RETN0(expr) \
  do { if((expr) == 0) { YARN_CHECK_ERR(); } } while(0)

#else

#define YARN_CHECK_RET0(expr) do { (expr); } while(0)
#define YARN_CHECK_RETN0(expr) do { (expr); } while(0)

#endif



#endif // YARN_HELPER_H_
