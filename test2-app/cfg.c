// https://stackoverflow.com/questions/30351725/llvm-loopinfo-in-functionpass-doesnt-compile
// #include <stdio.h>
//
// #define ARRAY_SIZE 100
//
// int foo(int* a , int n) {
  // int i;
  // int sum = 0;
  // for (; i < n; i++) {
    // printf("%s\n", "wwwww");
    // sum += a[i];
    // printf("%s\n", "wwwww");
  // }
  // return sum;
// }
//
// typedef void (*SignalHandler)();
//
// void bar(SignalHandler s){
  // s();
// }
//
// void sss(){
  // for (int i = 0; i < 12; ++i) {
    // int c;
    // scanf("%d", &c);
  // }
// }
//
//
// int main() {
  // int a[ARRAY_SIZE] = {1};
//
  // int sum = foo(a, ARRAY_SIZE);
//
  // bar(sss);
//
  // printf("sum:0x%x\n", sum);
  // return 0;
// }
