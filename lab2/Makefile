FLAGS=-lclangBasic -lclangAST -lclangBasic -lclangFrontend -lclangTooling -lLLVM -lclangSerialization


TEST_SRC = $(wildcard ../test2/*.c)
TEST_CASE = ${TEST_SRC:.c=.bc} 

a.out:LLVMAssignment.cpp
	clang++ LLVMAssignment.cpp $(FLAGS)

run: ${TEST_CASE}
	./a.out ../test2/test00.bc

%.bc: %.c
	clang -c -emit-llvm -o0 $< -o $@

bc:
	clang 

ra: ${TEST_CASE}
	 @for test in ${TEST_CASE}; do \
        echo $$test ; \
				./a.out $$test;\
   done
