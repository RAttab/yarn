/*!
\author Rémi Attab
\license FreeBSD (see the LICENSE file).
 */


#include "bits.h"


extern inline yarn_word_t yarn_bit_mask_range (yarn_word_t first, 
					       yarn_word_t second, 
					       yarn_word_t max);
extern inline yarn_word_t yarn_bit_log2 (yarn_word_t v);
extern inline yarn_word_t yarn_bit_trailing_zeros (yarn_word_t v);
