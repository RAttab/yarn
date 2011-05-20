/*!
\author Rémi Attab
\license FreeBSD (see the LICENSE file)

Linearly increasing and atomic timestamp.

Note that this timestamp only garantees consistency in the most common cases and the 
yarn_ts_comp function should only be used if the old_val and new_val were sampled in
reasonable time proximity. In other words, if the timestamp could have been increased
enough times overflow and come back to it's original value then that call will fail.

*/

#ifndef YARN_TIMESTAMP_H_
#define YARN_TIMESTAMP_H_


#include <types.h>
#include "atomic.h"


//! Atomic timestamp type. Only manipulate through the yarn_ts_xxx functions.
typedef yarn_atomic_var yarn_timestamp_t;


// Sets the highest order bit which is used as a flag to detect overflows.
#define YARN_TIMESTAMP_FLAG_MASK (1ULL << (sizeof(yarn_word_t)*8-1))


//!
inline void yarn_ts_init (yarn_timestamp_t* ts) {
  yarn_writev(ts, 0);
}

//!
inline void yarn_ts_free (yarn_timestamp_t* ts) {
  ((void) ts); //NOOP - Nothing to do in here.
}


//! Atomically reads the value of the timestamp. This is a memory barrier operation.
inline yarn_word_t yarn_ts_sample (yarn_timestamp_t* ts) {
  return yarn_readv(ts);
}


//! Atomically increments the timestamp.
inline yarn_word_t yarn_ts_inc (yarn_timestamp_t* ts) {
  return yarn_incv(ts);
}

//! Increments the timestamps if it's equal to old_val. All done atomicly.
inline bool yarn_ts_inc_eq (yarn_timestamp_t* ts, yarn_word_t old_val) {
  return yarn_casv(ts, old_val, old_val+1) == old_val;
}


/*!
Returns 0 if the timestamps are equal, a negative value if old_val is greater then new_val
and a positive value if new_val is greater then old_val.
*/
inline int yarn_ts_comp (yarn_word_t old_val, yarn_word_t new_val) {
  if (old_val == new_val) {
    return 0;
  }

  const yarn_word_t old_mask = old_val & YARN_TIMESTAMP_FLAG_MASK;
  const yarn_word_t new_mask = new_val & YARN_TIMESTAMP_FLAG_MASK;

  // This check only works in reasonable cases.
  if (old_mask == new_mask) {
    return new_val < old_val ? -1 : 1;
  }
  else {
    // If the flags are diferent then an overflow might have occured
    //  In any case, new_val is ahead since we can only increase linearly.
    return 1;
  }
}



#endif // YARN_STIMESTAMP_H_
