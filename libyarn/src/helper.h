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
  while(0) {		  \
    perror(__FUNCTION__); \
    assert(false);	  \
  } ((void)0)


#ifndef NDEBUG

#define YARN_CHECK_RET0(expr) \
  while(0) { if((expr) == 0) { YARN_CHECK_ERR(); } } ((void)0)
#define YARN_CHECK_RETN0(expr) \
  while(0) { if((expr) != 0) { YARN_CHECK_ERR(); } } ((void)0)

#else

#define YARN_CHECK_RET0(expr) while(0) { (expr); } ((void)0)
#define YARN_CHECK_RETN0(expr) while(0) { (expr); } ((void)0)

#endif



#endif // YARN_HELPER_H_
