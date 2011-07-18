/*
  loop header instrumentation.

  %yarn.s1 = alloca %yarn.t1
  %yarn.p1 = getelementptr %yarn.s1 0 0
  %yarn.p2 = getelementptr %yarn.s1 0 1
  store 0 %yarn.p1                            ; indvar init -> Same as phi node value.
  store %2 %yarn.p2
  %yarn.r1 = call yarn_exec_simple (%yarn.s1)
  %yarn.i1 = load yarn.p1
  %yarn.v1 = load yarn.p2
  %yarn.c1 = icmp %yarn.r1 == 0
  br %yarn.c1 %loopend, %loopbegin

  ; replace %2 by %yarn.v1 using %2's user list 
  ; Special care needed for induction variable since they can present purely in
  ;   a node. Gotta change that const by %yarn.i1.
*/


#include <stdio.h>

int f (int a, int* b, int c) {
  c = 10; 
  *b = a + *b + c;
  return 1;
}

int* ret_ptr (int a) {
  return &a;
}

void loop_simple (int a, int* b) {
  for (int i = 0; i < a; ++i) {
    *b += *b % 2 ? 5 : 7;
  }
}

void loop_break (int a, int* b) {
  for (int i = 0; i < a; ++i) {
    if (*b % 10) continue;
    if (*b % 11) break;
    *b += a;
  }

  (*b)++;
}

struct task {
  int* i;
  int* a;
  int* b;
};

int inst_loop (void* p) {
  struct task* t = (struct task*) p;
  
  if (*t->i < *t->a) {
    return 0;
  }

  if (*t->b % 2) {
    *t->b += 5;
  }
  else  {
    *t->b += 7;
  }

  return 1;
}

int main (int argc, char** argv) {
  
  int a = 1;
  int b = 2;
  int* c = &b;

  int d = f(a,c, 10);

  int* e = ret_ptr(a);

  printf("a=%d, b=%d, c=%p", a, b, c);
  
  return 0;
}
