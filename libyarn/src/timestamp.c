/*!
\author Rémi Attab
\license FreeBSD (see the LICENSE file)

Provides the external definitions for inline functions in case the compiler doesn't 
inline or if the user tries to grab the address.
*/

#include "timestamp.h"

extern inline bool yarn_timestamp_init (yarn_timestamp_t* ts);
extern inline void yarn_timestamp_destroy (yarn_timestamp_t* ts);

extern inline yarn_word_t yarn_timestamp_sample (yarn_timestamp_t* ts);

extern inline yarn_word_t yarn_timestamp_inc (yarn_timestamp_t* ts);
extern inline bool yarn_timestamp_inc_eq (yarn_timestamp_t* ts, yarn_word_t old_val);

extern inline int yarn_timestamp_comp (yarn_word_t old_val, yarn_word_t new_val);

