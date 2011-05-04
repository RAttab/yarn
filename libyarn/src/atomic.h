/*!
\author Rémi Attab
\license FreeBSD (see LICENSE file)

\brief Collection of atomic primitives and utilities.

These are currently defined only for GCC using it's builtin functions. Will need to be
ported to other compilers if we ever get there.

\todo Should look into forcing the inlining.

*/


#ifdef YARN_ATOMIC_H_
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

/*!
Type for atomic pointers.
No guarantees are made about the size except that it's enough to hold a pointer.
 */
typedef uintprt_t yarn_atomp_t;


/*!
Basic types for atomic variables and pointers.
The struct is provided as a safety proxy and should never be manipulated directly. 
Use the functions in the concurrency.h header instead. 
 */
struct yarn_atomic_var { 
  volatile yarn_atomv_t var; 
};
struct yarn_atomic_ptr {
  volatile yarn_atomp_t ptr;
};


//! Atomically reads the variable.
inline yarn_atomv_t yarn_readv(struct yarn_atomic_var* a) {return a->var;}
inline void* yarn_readp(struct yarn_atomic_ptr* a) {return (void*)a->ptr;}

//! Atomically writes the variable.
//! \todo Need to make this a full memory barrier op.
inline void yarn_writev(struct yarn_atomic_var* a, yarn_atomv_t var) {a->val = var;}
inline void yarn_writep(struct yarn_atomic_ptr* a, void* ptr) {
  a->ptr = (yarn_atomp_t) ptr;
}


//! Atomically increments the variable.
inline void yarn_incv(struct yarn_atomic_var* a) {
  __sync_add_and_fetch(&a->var, 1);
}
//! Atomically decrements the variable.
inline void yarn_decv(struct yarn_atomic_var* a) {
  __sync_sub_and_fetch(&a->var, 1);
}


//! Compare and swap which returns the value currently in the variable.
inline yarn_atomv_t yarn_casv (struct yarn_atomic_var* a, 
			       yarn_atomv_t oldval, 
			       yarn_atomv_t newval) 
{
  return __sync_val_compare_and_swap(&a->var, oldval, newval);
}

inline yarn_atomp_t yarn_casp (struct yarn_atomic_ptr* a,
			       void* oldval,
			       void* newval)
{
  return (void*) __sync_val_compare_and_swap(&a->ptr, 
					     (yarn_atomp_t)oldval, 
					     (yarn_atomp_t)newval);
}

/*!
Spins until the atomic var is equal to the expected value.
Should only be used when expecting a short waiting period.
\todo Could add a yield or wait(0) if we're spinning for too long. Could use a timeout too.
*/
inline void yarn_spinv_eq (struct yarn_atomic_var* a, yarn_atomv_t newval) {
  while(yarn_readv(a) != newval);
}
inline void yarn_spinv_neq (struct yarn_atomic_var* a, yarn_atomv_t oldval) {
  while(yarn_readv(a) == oldval);
}

inline void yarn_spinp_eq (struct yarn_atomic_ptr* a, yarn_atomp_t newptr) {
  while(yarn_readp(a) != newptr);
}
inline void yarn_spinp_neq (struct yarn_atomic_ptr* a, yarn_atomp_t oldptr) {
  while(yarn_readp(a) == oldptr);
}


#endif // YARN_ATOMIC_H_
