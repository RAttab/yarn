/*!
\author Rémi Attab
\license FreeBSD (see the LICENSE file)

Provides the external definitions for inline functions in case the compiler doesn't 
inline or if the user tries to grab the address.
*/

#include "timestamp.h"

extern inline void yarn_ts_init (yarn_timestamp_t* ts);
extern inline void yarn_ts_free (yarn_timestamp_t* ts);

extern inline yarn_ts_sample_t yarn_ts_sample (yarn_timestamp_t* ts);

extern inline void yarn_ts_inc (yarn_timestamp_t* ts);
extern inline bool yarn_ts_inc_eq (yarn_timestamp_t* ts, yarn_ts_sample_t old_val);

extern inline int yarn_ts_comp (yarn_ts_sample_t old_val, yarn_ts_sample_t new_val);

