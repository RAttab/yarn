#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>


typedef uint_fast32_t word_t;


word_t sum (word_t n) {
  word_t acc = 0;
  for (word_t i = 1; i <= n; ++i) {
    acc += i;
  }
  return acc;
}

word_t fast_sum(word_t n) {
  return (n*(n+1))/2;
}


int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "Missing argument.\n");
    return 1;
  }


  word_t n = strtol(argv[1], NULL, 10);
  word_t slow = sum(n);
  word_t fast = fast_sum(n);
  printf("[%zu] sum=%zu, fast_sum=%zu\n", n, slow, fast);
  
  return 0;
}
