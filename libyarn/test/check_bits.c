/*!
\author Rémi Attab
\license FreeBSD (see license file).
*/


#include "check_libyarn.h"

#include <bits.h>

#include <stdio.h>


START_TEST(t_bits_range_mask) {
  {
    yarn_word_t mask = yarn_bit_mask_range(0,0);
    fail_if(mask != 0, "(%zu, %zu), mask=%zu, expected=%zu", 0, 0, mask, 0);
  }

  {
    for (size_t i = 0; i < YARN_WORD_BIT_SIZE; ++i) {
      yarn_word_t expected = 0;

      for (size_t j = 0; j < YARN_WORD_BIT_SIZE-1; ++j) {
	size_t k = j + i + 1;
	expected |= ((yarn_word_t)1) << (YARN_BIT_INDEX(k)-1);

	yarn_word_t mask = yarn_bit_mask_range(i, k);

	fail_if(mask != expected, "(%zu, %zu), mask=%zu, exptected=%zu",
		i, k, mask, expected);
      }
    }

  }
}
END_TEST

START_TEST(t_bits_log2) {
  {
    yarn_word_t log2 = yarn_bit_log2(0);
    fail_if(log2 != 0, "word=%zu, log2=%zu, expected=%zu", 0, log2, 0);
  }

  {
    yarn_word_t word = 0;
    for (size_t i = 0; i < YARN_WORD_BIT_SIZE; ++i) {
      word = (word << 1) + 1;

      yarn_word_t log2 = yarn_bit_log2(word);
      fail_if(log2 != i, 
	      "i=%zu, word=%zu, log2=%zu, expected=%zu", i, word, log2, i+1);

    }
  }
}
END_TEST

START_TEST(t_bits_trailing_zeros) {
  // The returned value is incorrect (should be 64).
  // We currently don't need to call this with 0 as an input so it's fine for the moment.
  {
    yarn_word_t zeros = yarn_bit_trailing_zeros(0);
    fail_if(zeros != YARN_WORD_BIT_SIZE-1, 
	    "zeros=%zu, expected=%zu", zeros, YARN_WORD_BIT_SIZE-1);
  }

  {
    yarn_word_t word = -1;
    for (size_t i = 0; i < YARN_WORD_BIT_SIZE; ++i) {

      yarn_word_t zeros = yarn_bit_trailing_zeros(word);
      fail_if(zeros != i, 
	      "i=%zu, word=%zu, zeros=%zu, expected=%zu", i, word, zeros, i);

      word <<= 1;
    }
  }
}
END_TEST


Suite* yarn_bits_suite (void) {
  Suite* s = suite_create("yarn_bits");

#ifdef YARN_WORD_64
  printf("yarn_bits -> Running 64 bits tests.\n");
#else
  printf("yarn_bits -> Running 32 bits tests.\n");
#endif

  TCase* tc_basic = tcase_create("yarn_bits");
  tcase_add_test(tc_basic, t_bits_range_mask);
  tcase_add_test(tc_basic, t_bits_log2);
  tcase_add_test(tc_basic, t_bits_trailing_zeros);
  suite_add_tcase(s, tc_basic);

  return s;
}

