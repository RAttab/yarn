/*!
\author Rémi Attab
\license FreeBSD (see the LICENSE file).

Bit field manipulation and bit hacks.

Everything in here operates on the yarn_word_t type. Any provided index will be trunk to
an appropriate size.
 */


#ifndef YARN_BITS_H_
#define YARN_BITS_H_


#include <types.h>


#define YARN_BIT_INDEX(value) ((value) % (sizeof(yarn_word_t)*8))
#define YARN_BIT_MASK(index) (((yarn_word_t)1) << YARN_BIT_INDEX(index))
#define YARN_BIT_SET(word,index) ((word) | YARN_BIT_MASK(index))
#define YARN_BIT_CLEAR(word,index) ((word) & ~YARN_BIT_MASK(index))


inline yarn_word_t yarn_bit_mask_range (yarn_word_t first, yarn_word_t second) {
  yarn_word_t first_index = YARN_BIT_INDEX(first);
  yarn_word_t second_index = YARN_BIT_INDEX(second);

  yarn_word_t b = (first_index-1) ^ (second_index-1);

  return first_index <= second_index ? b : ~b;
}



#endif // YARN_BITS_H_
