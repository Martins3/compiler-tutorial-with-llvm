FLAGS=-lclangBasic -lclangAST -lclangBasic -lclangFrontend -lclangTooling -lLLVM -lclangSerialization -Wall


LIBS=clangBasic clangAST clangBasic clangFrontend clangTooling clangSerialization
LIB_DIR=/run/media/shen/Will/Compiler/llvm-project/build/lib
LIBS_PATH=$(addsuffix .a,$(addprefix $(LIB_DIR)/lib, $(LIBS))) ${LIB_DIR}/libclang.so.10




TEST_SRC = $(wildcard ../test2/*.c ../test2-app/*.c)
TEST_CASE = ${TEST_SRC:.c=.bc} 
RUN_DEP=${TEST_CASE} a.out

a.out:LLVMAssignment.cpp
	clang++ LLVMAssignment.cpp $(FLAGS)
	@# clang++ LLVMAssignment.cpp $(LIBS_PATH)

#----------------------------
run: ${RUN_DEP}
	./a.out ../test2-app/case.bc

debug: ${RUN_DEP}
	gdb ./a.out -x gdbinit

clean:
	rm ../test2-app/case.bc


cp:
	cp ../test2/test15.c ../test2-app/case.c
#----------------------------


test: ${TEST_CASE}
	@echo "refresh case"

%.bc: %.c
	clang -c -emit-llvm -O0 $< -o $@

ra: ${RUN_DEP}
	 @for test in ${TEST_CASE}; do \
        echo $$test ; \
				./a.out $$test;\
   done
