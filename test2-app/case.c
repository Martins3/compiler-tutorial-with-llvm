#include <stdlib.h>

int plus(int a, int b) {
   return a+b;
}
//
// int minus(int a,int b)
// {
    // return a-b;
// }

typedef  int(* FP)(int, int);

FP gg(){
  return plus;
}

int foo(int a,int b,int(* a_fptr)(int, int))
{
    FP fp = gg();
    a_fptr = fp;

    a_fptr(a, b);
    return fp(a,b);
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
//14 : plus, minus
//24 : foo
//27 : foo 
