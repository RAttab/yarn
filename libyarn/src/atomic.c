/*!
\author Rémi Attab
\license FreeBSD (see LICENSE file)

Provides the external definitions for inline functions in case the compiler doesn't 
inline or if the user tries to grab the address.
 */

#include "atomic.h"


extern inline yarn_atomv_t yarn_readv(yarn_atomic_var* a);
extern inline void* yarn_readp(yarn_atomic_ptr* a);

extern inline yarn_atomv_t yarn_readv_barrier(yarn_atomic_var* a);
extern inline void* yarn_readp_barrier(yarn_atomic_ptr* a);

extern inline void yarn_writev(yarn_atomic_var* a, yarn_atomv_t var);
extern inline void yarn_writep(yarn_atomic_ptr* a, void* ptr);

extern inline void yarn_writev_barrier(yarn_atomic_var* a, yarn_atomv_t var);
extern inline void yarn_writep_barrier(yarn_atomic_ptr* a, yarn_atomp_t ptr);

extern inline yarn_atomv_t yarn_incv(yarn_atomic_var* a);
extern inline yarn_atomp_t yarn_decv(yarn_atomic_var* a);

extern inline yarn_atomv_t yarn_casv (yarn_atomic_var* a, 
				      yarn_atomv_t oldval, 
				      yarn_atomv_t newval);
extern inline void* yarn_casp (yarn_atomic_ptr* a, void* oldval, void* newval);

extern inline yarn_atomv_t yarn_casv_fast (yarn_atomic_var* a,
					   yarn_atomv_t oldval, 
					   yarn_atomv_t newval);
extern inline void* yarn_casp_fast (yarn_atomic_ptr* a, void* oldval, void* newval);

extern inline void yarn_spinv_eq (yarn_atomic_var* a, yarn_atomv_t newval);
extern inline void yarn_spinv_neq (yarn_atomic_var* a, yarn_atomv_t oldval);
extern inline void yarn_spinp_eq (yarn_atomic_ptr* a, void* newptr);
extern inline void yarn_spinp_neq (yarn_atomic_ptr* a, void* oldptr);



