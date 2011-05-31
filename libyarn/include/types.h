/*!
\author Rémi Attab
\license FreeBSD (see the LICENSE file)

 */


#ifndef YARN_TYPES_H_
#define YARN_TYPES_H_


#include <stdint.h>
#include <stdbool.h>
#include <stddef.h> // size_t


typedef uint_fast32_t yarn_word_t;


#define YARN_WORD_MAX UINT_FAST32_MAX


#if (YARN_WORD_MAX == UINT64_MAX)
#  define YARN_WORD_64
#else
#  undef YARN_WORD_64
#endif


#define YARN_WORD_BIT_SIZE (sizeof(yarn_word_t)*8)


#endif // YARN_TYPES_H_
