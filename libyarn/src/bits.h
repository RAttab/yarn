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

#if (YARN_WORD_MAX == UINT64_MAX)
#  define YARN_BIT_64
#else
#  undef YARN_BIT_64
#endif


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


/*!
\todo Probably won't be inlined because of the table. Fix it.

Original taken from the Stanford Bit Twidling hacks page and extended for 64 bits.
http://www-graphics.stanford.edu/~seander/bithacks.html#IntegerLogLookup
*/
inline yarn_word_t yarn_bit_log2 (yarn_word_t v) {
  static const char log_tables_256[256] = {

#define YARN_BIT_LT(n) n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n

    -1, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
    YARN_BIT_LT(4), YARN_BIT_LT(5), YARN_BIT_LT(5), 
    YARN_BIT_LT(6), YARN_BIT_LT(6), YARN_BIT_LT(6), YARN_BIT_LT(6),
    YARN_BIT_LT(7), YARN_BIT_LT(7), YARN_BIT_LT(7), YARN_BIT_LT(7), 
    YARN_BIT_LT(7), YARN_BIT_LT(7), YARN_BIT_LT(7), YARN_BIT_LT(7)
  };

  register unsigned int t, tt; // temporaries

#ifdef YARN_BIT_64
  tt = v >> 48;
  if (tt) {
    return (t = tt >> 8) ? 56 + log_tables_256[t] : 48 + log_tables_256[tt];    
  }
  tt = v >> 32;
  if (tt) {
    return (t = tt >> 8) ? 40 + log_tables_256[t] : 32 + log_tables_256[tt];
  }
#endif
  tt = v >> 16;
  if (tt) {
    return (t = tt >> 8) ? 24 + log_tables_256[t] : 16 + log_tables_256[tt];
  }
  return (t = (v >> 8)) ? 8 + log_tables_256[t] : log_tables_256[v];
}


/*!
\todo Probably won't be inlined but need to double check.

Original taken from the Stanford Bit Twidling hacks page and extended for 64 bits.
http://www-graphics.stanford.edu/~seander/bithacks.html#ZerosOnRightBinSearch
*/
inline yarn_word_t yarn_bit_trailing_zeros (yarn_word_t v) {
  // NOTE: if 0 == v, then c = 31.
  if (v & 0x1) {
    // special case for odd v (assumed to happen half of the time)
    return 0;
  }

  else {
    yarn_word_t c = 1;

#ifdef YARN_BIT_64
    if ((v & 0xffffffff) == 0) {
      v >>= 32;
      c += 32;
    }
#endif
    if ((v & 0xffff) == 0) {  
      v >>= 16;  
      c += 16;
    }
    if ((v & 0xff) == 0) {  
      v >>= 8;  
      c += 8;
    }
    if ((v & 0xf) == 0) {  
      v >>= 4;
      c += 4;
    }
    if ((v & 0x3) == 0) {  
      v >>= 2;
      c += 2;
    }
    c -= v & 0x1;
    return c;
  }
}


#endif // YARN_BITS_H_