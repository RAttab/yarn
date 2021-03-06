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


#include "yarn/types.h"
#include "atomic.h"


//! Atomic timestamp type. Only manipulate through the yarn_timestamp_xxx functions.
typedef yarn_atomic_var yarn_timestamp_t;


// Sets the highest order bit which is used as a flag to detect overflows.
#define YARN_TIMESTAMP_FLAG_MASK (3ULL << (YARN_WORD_BIT_SIZE - 2))


//!
inline bool yarn_timestamp_init (yarn_timestamp_t* ts) {
  yarn_writev(ts, 0);
  return true;
}

//!
inline void yarn_timestamp_destroy (yarn_timestamp_t* ts) {
  ((void) ts); //NOOP - Nothing to do in here.
}


//! Atomically reads the value of the timestamp. This is a memory barrier operation.
inline yarn_word_t yarn_timestamp_sample (yarn_timestamp_t* ts) {
  return yarn_readv(ts);
}


//! Atomically increments the timestamp.
inline yarn_word_t yarn_timestamp_inc (yarn_timestamp_t* ts) {
  return yarn_incv(ts);
}

//! Increments the timestamps if it's equal to old_val. All done atomicly.
inline bool yarn_timestamp_inc_eq (yarn_timestamp_t* ts, yarn_word_t old_val) {
  return yarn_casv(ts, old_val, old_val+1) == old_val;
}


/*!
Returns 0 if the timestamps are equal, a negative value if old_val is smaller then new_val
and a positive value if new_val is smaller then old_val.
*/
inline int yarn_timestamp_comp (yarn_word_t a, yarn_word_t b) {
  if (a == b) {
    return 0;
  }

  const yarn_word_t a_mask = a & YARN_TIMESTAMP_FLAG_MASK;
  const yarn_word_t b_mask = b & YARN_TIMESTAMP_FLAG_MASK;

  // This check only works in reasonable cases.
  if (a_mask == b_mask) {
    return a < b ? -1 : 1;
  }
  else if (a_mask == 0) {
    return b_mask == YARN_TIMESTAMP_FLAG_MASK ? 1 : -1;
  }
  else if (b_mask == 0) {
    return a_mask == YARN_TIMESTAMP_FLAG_MASK ? -1 : 1;
  }
  else {
    return a_mask < b_mask ? -1 : 1;
  }
}



#endif // YARN_STIMESTAMP_H_
