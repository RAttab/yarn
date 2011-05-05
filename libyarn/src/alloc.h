/*!
\author Rémi Attab
\license FreeBSD (see LICENSE file)

\brief Proxy header for malloc.

This header is used to proxy any call to malloc in case we ever want to switch to a
pool allocator like tcmalloc.

*/ 


#ifndef YARN_ALLOC_H_
#define YARN_ALLOC_H_


#include <stdlib.h>
#include <stddef.h>


// Implementation details, use YARN_PTR_ALIGN instead.
#define YARN_PTR_ALIGN_CALC(boundary) \
  ((boundary) / sizeof(void*))


//! Returns the platform dependant alignment for a byte alignment.
#define YARN_PTR_ALIGN(boundary) \
  (YARN_PTR_ALIGN_CALC(boundary) < 1 ? 1 : YARN_PTR_ALIGN_CALC(boundary))



//! \see malloc(size_t)
inline void* yarn_malloc (size_t size) {
  return malloc (size);
}


/*!
\see posix_memalign(void*,size_t,size_t)

\todo Error handling.
  - EINVAL -> Bad alignment val.
  - ENOMEM -> out of mem.
*/
inline void* yarn_memalign (size_t alignment, size_t size) {

  void* ptr;
  int err = posix_memalign(&ptr, alignment, size);
  
  return !err ? ptr : NULL;

}


//! \see free(void*)
inline void yarn_free (void* ptr) {
  (void) free(ptr);
}



/*! 
\todo Pool allocators

We don't really need a full pool alocator like tc malloc since we pretty much always only
need 1 extra temp var per thread. Easier implementation would be to simply create an array
where each element belongs to one trhead and contains a pre-allocated object.


 */


#endif //YARN_ALLOC_H_
