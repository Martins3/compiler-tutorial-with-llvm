extern int GET();
extern void * MALLOC(int);
extern void FREE(void *);
extern void PRINT(int);

int main() {
  char a = 12;
  char b = 12;
  // PRINT(a + b);
  // PRINT(a - b);
  char * x = (char *)MALLOC(sizeof(char));
  *x = 12;
  PRINT(*x);
}
