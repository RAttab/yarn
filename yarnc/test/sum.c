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
  
  word_t n = 10000;
  word_t slow = sum(n);
  word_t fast = fast_sum(n);
  printf("[%zu] sum=%zu, fast_sum=%zu\n", n, slow, fast);

}
