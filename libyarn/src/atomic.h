/*!
\author Rémi Attab
\license FreeBSD (see LICENSE file)

\brief Collection of atomic primitives and utilities.

These are currently defined only for GCC using it's builtin functions. Will need to be
ported to other compilers if we ever get there.

\todo Should look into forcing the inlining.

*/


#ifndef YARN_ATOMIC_H_
#define YARN_ATOMIC_H_


#include <stdint.h>
#include <stdbool.h>


//! Defines a full memory barrier.
#define yarn_mem_barrier() __sync_synchronize()


/*! 
Type for atomic variables.
The size will change depending on the platform but is guaranteed to be at least 32 bits.
 */
typedef uint_fast32_t yarn_atomv_t;
#define YARN_ATOMV_UNLIKELY UINT_FAST32_MAX

/*!
Type for atomic pointers.
No guarantees are made about the size except that it's enough to hold a pointer.
 */
typedef uintptr_t yarn_atomp_t;
#define YARN_ATOMP_UNLIKELY UINTPTR_MAX


/*!
Basic types for atomic variables and pointers.
The struct is provided as a safety proxy and should never be manipulated directly. 
Use the functions in the concurrency.h header instead. 
 */
typedef struct { 
  volatile yarn_atomv_t var; 
} yarn_atomic_var;
typedef struct {
  volatile yarn_atomp_t ptr;
} yarn_atomic_ptr;


//! Atomically reads the variable.
inline yarn_atomv_t yarn_readv(yarn_atomic_var* a) {
  return a->var;
}
inline void* yarn_readp(yarn_atomic_ptr* a) {
  return (void*)a->ptr;
}

//! Atomically reads the variable while making sure no memory ops can cross the call.
inline yarn_atomv_t yarn_readv_barrier(yarn_atomic_var* a) {
  return __sync_val_compare_and_swap(&a->var, 
				     YARN_ATOMV_UNLIKELY, 
				     YARN_ATOMV_UNLIKELY);
}
inline void* yarn_readp_barrier(yarn_atomic_ptr* a) {
  return (void*)__sync_val_compare_and_swap(&a->ptr, 
					    YARN_ATOMP_UNLIKELY, 
					    YARN_ATOMP_UNLIKELY);
}

//! Atomically writes the variable.
inline void yarn_writev(yarn_atomic_var* a, yarn_atomv_t var) {
  a->var = var;
}
inline void yarn_writep(yarn_atomic_ptr* a, void* ptr) {
  a->ptr = (yarn_atomp_t) ptr;
}

//! Atomically writes the variable while making sure no memory ops can cross the call.
inline void yarn_writev_barrier(yarn_atomic_var* a, yarn_atomv_t var) {
  yarn_mem_barrier(); //! \todo Should be Release semantic
  a->var = var;
  yarn_mem_barrier(); //! \todo Should be Acquire semantic
}
inline void yarn_writep_barrier(yarn_atomic_ptr* a, yarn_atomp_t ptr) {
  yarn_mem_barrier();
  a->ptr = (yarn_atomp_t) ptr;
  yarn_mem_barrier();
}


//! Atomically increments the variable.
inline yarn_atomv_t yarn_incv(yarn_atomic_var* a) {
  return __sync_add_and_fetch(&a->var, 1);
}
//! Atomically decrements the variable.
inline yarn_atomp_t yarn_decv(yarn_atomic_var* a) {
  return __sync_sub_and_fetch(&a->var, 1);
}


//! Compare and swap which returns the value of the variable before the swap.
inline yarn_atomv_t yarn_casv (yarn_atomic_var* a, 
			       yarn_atomv_t oldval, 
			       yarn_atomv_t newval) 
{
  return __sync_val_compare_and_swap(&a->var, oldval, newval);
}

inline void* yarn_casp (yarn_atomic_ptr* a, void* oldval, void* newval) {
  return (void*) __sync_val_compare_and_swap(&a->ptr, 
					     (yarn_atomp_t)oldval, 
					     (yarn_atomp_t)newval);
}

/*!
Compare and swap which returns the value of the variable before the swap.
This version attempts to skip the memory barrier by doing a barrier free check
on the value. Note that this screws with the linearilization point so care must be
taken when used.
 */
inline yarn_atomv_t yarn_casv_fast (yarn_atomic_var* a,
				    yarn_atomv_t oldval, 
				    yarn_atomv_t newval) 
{
  yarn_atomv_t var = a->var;
  if (var != oldval)
    return var;

  return __sync_val_compare_and_swap(&a->var, oldval, newval);
}

inline void* yarn_casp_fast (yarn_atomic_ptr* a, void* oldval, void* newval) {
  void* ptr = (void*) a->ptr;
  if (ptr != oldval)
    return ptr;

  return (void*) __sync_val_compare_and_swap(&a->ptr, 
					     (yarn_atomp_t)oldval, 
					     (yarn_atomp_t)newval);
}

				    

/*!
Spins until the atomic var is equal to the expected value.
Should only be used when expecting a short waiting period.
These ops are also a full memory barriers.
\todo Could add a yield or wait(0) if we're spinning for too long. Could use a timeout too.
*/
inline void yarn_spinv_eq (yarn_atomic_var* a, yarn_atomv_t newval) {
  yarn_mem_barrier(); //! \todo Should be Release semantic
  while(a->var != newval);
  yarn_mem_barrier(); //! \todo Should be Acquisition semantic
}
inline void yarn_spinv_neq (yarn_atomic_var* a, yarn_atomv_t oldval) {
  yarn_mem_barrier();
  while(a->var == oldval);
  yarn_mem_barrier();
}

inline void yarn_spinp_eq (yarn_atomic_ptr* a, void* newptr) {
  yarn_mem_barrier();
  while(a->ptr != (yarn_atomp_t)newptr);
  yarn_mem_barrier();
}
inline void yarn_spinp_neq (yarn_atomic_ptr* a, void* oldptr) {
  yarn_mem_barrier();
  while(a->ptr == (yarn_atomp_t)oldptr);
  yarn_mem_barrier();
}


#endif // YARN_ATOMIC_H_
