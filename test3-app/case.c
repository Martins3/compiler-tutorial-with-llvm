#include <stdlib.h>
struct fptr
{
  int (*p_fptr)(int, int);
  int m;
};

struct sfptr{
  struct fptr * a;
};

int plus(int a, int b) {
   return a+b;
}

int minus(int a, int b) {
   return a-b;
}

int (*foo(int a, int b, struct fptr * c_fptr, struct fptr * d_fptr))(int, int) {
   return c_fptr->p_fptr;
}

int (*clever(int a, int b, struct fptr * c_fptr, struct fptr * d_fptr))(int, int) {
  if(a>0 && b<0)
  {
   return d_fptr->p_fptr;
  }
  return c_fptr->p_fptr;
}


typedef  int (*GG)(int, int);


void K(GG a, GG b, int x){
  GG * s;
  if(x){
    s = &b;
  }else{
   s = &a;
  }

  if(x){
    *s = minus;
  }

  a(1, 2);
}

void KK(int x){
  int a = 1;
  int b = 2;
  int * gg;
  if(x){
    gg = &a;
  }else{
    gg = &b;
  }
  *gg = 1111111;
}

int moo(char x, int op1, int op2) {

  struct fptr a;
  a.p_fptr = minus;
  a.p_fptr(1, 2);

  a.m =  88888888;
  struct sfptr g;
  g.a = &a;
  g.a->p_fptr(1, 2);
  return 0;
}

// 41 : foo, clever
// 42 : plus, minus
