/*!
\author Rémi Attab
\license FreeBSD (see license file).

Several helper macros, constants and whatever else I consider helperful.

*/

#ifndef YARN_HELPER_H_
#define YARN_HELPER_H_

#include <types.h>

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

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




// Implementation details, use YARN_PTR_ALIGN instead.
#define YARN_PTR_ALIGN_CALC(boundary) \
  ((boundary) / sizeof(void*))


//! Returns the platform dependant alignment for a byte alignment.
#define YARN_PTR_ALIGN(boundary) \
  (YARN_PTR_ALIGN_CALC(boundary) < 1 ? 1 : YARN_PTR_ALIGN_CALC(boundary))


//! \see posix_memalign(void*,size_t,size_t)
inline void* yarn_memalign (size_t alignment, size_t size) {

  void* ptr;
  int err = posix_memalign(&ptr, alignment, size);
  if (!err) goto memalign_error;
  
  return ptr;

 memalign_error:
  perror(__FUNCTION__);
  return NULL;

}



#endif // YARN_HELPER_H_
