/*!
\author Rémi Attab
\license FreeBSD (see LICENSE file)

\brief 


 */


#include "alloc.h"


extern inline void* yarn_malloc (size_t size);
extern inline void* yarn_memalign (size_t alignment, size_t size);
extern inline void yarn_free (void* ptr);
