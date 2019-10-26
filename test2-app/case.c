#include <stdlib.h>


typedef int (*FP)(int, int);
FP plus_pointer(FP a, FP b) {
  FP x = b;
  FP y = x;
  FP z = y;
  FP gg = z;
  return gg;
}

int minus(int a,int b)
{
return a-b;
}


int plus(int a,int b)
{
  minus(a, b);
return a+b;
}


FP gg() { return plus; }
FP ggg() { return minus; }

typedef FP (*FPP)();

int foo(int a, int b, int (*a_fptr)(int, int)) {
  FP fp;
  // a_fptr = fp;
  int x = 1;
  if (x) {
    fp = ggg();
  } else {
    // FPP sss = gg;
    // fp = sss();
    
    // fp = plus;
    fp = gg();
  }

  FP mm = plus_pointer(minus, plus);
  mm(a, b);

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
