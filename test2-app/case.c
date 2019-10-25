#include <stdlib.h>

int *m(int *x) { return x; }

int plus(int a, int b) {
  int x = a;
  int y = x;
  int z = y;
  int gg = a;

  return gg;
}
//
// int minus(int a,int b)
// {
// return a-b;
// }

typedef int (*FP)(int, int);

FP gg() { return plus; }

int foo(int a, int b, int (*a_fptr)(int, int)) {
  FP fp;
  // a_fptr = fp;
  int x = 1;
  if (x) {
    fp = NULL;
  } else {
    fp = gg();
  }

  fp(a, b);
  return fp(a, b);
}

// int moo(char x)
// {
// int (*af_ptr)(int ,int ,int(*)(int, int))=foo;
// int (*pf_ptr)(int,int)=0;
// if(x == '+'){
// pf_ptr=plus;
// af_ptr(1,2,pf_ptr);
// pf_ptr=minus;
// }
// af_ptr(1,2,pf_ptr);
// return 0;
// }
// 14 : plus, minus
// 24 : foo
// 27 : foo
