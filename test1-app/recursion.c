extern int GET();
extern void * MALLOC(int);
extern void FREE(void *);
extern void PRINT(int);

int fib(int x){
  if (x <= 1){
    return 1;
  }

  return fib(x - 1) + fib(x - 2);
}

int nothing(int x){
  return x;
}

int main() {
  // int x = 1;
  // PRINT(nothing(x - 1));
  PRINT(fib(4));
}
