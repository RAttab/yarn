/*!
\author Rémi Attab
\license FreeBSD (see license file).

Several helper macros, constants and whatever else I consider helperful.

*/

#ifndef YARN_HELPER_H_
#define YARN_HELPER_H_

#include "yarn/types.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>


// Prints a 64 bit hexadecimal value.
//! \todo Needs fixing for 32 bit.
#ifdef YARN_WORD_64
#  define YARN_SHEX "0x%08x%08x"
#  define YARN_AHEX(v) (uint32_t)(((yarn_word_t)v)>>32), (uint32_t)(v)
#else
#  define YARN_SHEX "0x%08x"
#  define YARN_AHEX(v) (v)
#endif



// Error checking macros

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
