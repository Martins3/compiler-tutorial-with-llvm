#include <stdlib.h>

int plus(int a, int b) {
   return a+b;
}

int minus(int a,int b)
{
    return a-b;
}


typedef int(*GG  )(int, int);

void K(int x){
  GG a = minus;
  GG b = plus;
  GG * s;
  if(x){
    s = &b;
  }else{
   s = &a;
  }
  *s = plus;
  a(1, 2);
}

//14 : plus, minus
//24 : foo
//27 : foo

