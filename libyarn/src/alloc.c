/*!
\author Rémi Attab
\license FreeBSD (see LICENSE file)

Provides the external definitions for inline functions in case the compiler doesn't 
inline or if the user tries to grab the address.
 */


#include "alloc.h"


extern inline void* yarn_malloc (size_t size);
extern inline void* yarn_memalign (size_t alignment, size_t size);
extern inline void yarn_free (void* ptr);

