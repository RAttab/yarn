/*!
\author Rémi Attab
\license FreeBSD (see LICENSE file)

Provides the external definitions for inline functions in case the compiler doesn't 
inline or if the user tries to grab the address.
 */

#include "atomic.h"



extern inline yarn_atomv_t yarn_readv(struct yarn_atomic_var* a);
extern inline void* yarn_readp(struct yarn_atomic_ptr* a);

extern inline void yarn_writev(struct yarn_atomic_var* a, yarn_atomv_t var);
extern inline void yarn_writep(struct yarn_atomic_ptr* a, void* ptr);

extern inline void yarn_incv(struct yarn_atomic_var* a);
extern inline void yarn_decv(struct yarn_atomic_var* a);


extern inline yarn_atomv_t yarn_casv (struct yarn_atomic_var* a, 
				      yarn_atomv_t oldval, 
				      yarn_atomv_t newval);
extern inline yarn_atomp_t yarn_casp (struct yarn_atomic_ptr* a,
				      void* oldval,
				      void* newval);

extern inline yarn_atomv_t yarn_casv_fast (struct yarn_atomic_var* a,
					   yarn_atomv_t oldval, 
					   yarn_atomv_t newval) 
extern inline yarn_atomp_t yarn_casp_fast (struct yarn_atomic_ptr* a,
					   void* oldval, 
					   void* newval) 



extern inline void yarn_spinv_eq (struct yarn_atomic_var* a, yarn_atomv_t newval);
extern inline void yarn_spinv_neq (struct yarn_atomic_var* a, yarn_atomv_t oldval);
extern inline void yarn_spinp_eq (struct yarn_atomic_ptr* a, yarn_atomp_t newptr);
extern inline void yarn_spinp_neq (struct yarn_atomic_ptr* a, yarn_atomp_t oldptr);



