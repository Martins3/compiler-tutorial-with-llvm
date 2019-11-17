FLAGS=-lclangBasic -lclangAST -lclangFrontend -lclangTooling -lLLVM -lclangSerialization -Wall
# FLAGS=$(llvm-config --cxxflags --ldflags --libs)


LIBS=clangBasic clangAST clangBasic clangFrontend clangTooling clangSerialization
LIB_DIR=/run/media/shen/Will/Compiler/llvm-project/build/lib
LIBS_PATH=$(addsuffix .a,$(addprefix $(LIB_DIR)/lib, $(LIBS))) ${LIB_DIR}/libclang.so.10




TEST_SRC = $(wildcard ../test2/*.c ../test2-app/*.c)
TEST_CASE = ${TEST_SRC:.c=.bc} 
RUN_DEP=${TEST_CASE} a.out

DEBUG_ME=../test2-app/case.bc
# DEBUG_ME=../test2-app/cfg.bc
# DEBUG_ME=../test2/test14.bc

a.out:LLVMAssignment.cpp
	clang++ -g LLVMAssignment.cpp $(FLAGS)
	@# clang++ LLVMAssignment.cpp $(LIBS_PATH)

#----------------------------
run: ${RUN_DEP}
	./a.out ${DEBUG_ME}

debug: ${RUN_DEP}
	gdb ./a.out -x gdbinit

clean:
	rm -f ${TEST_CASE}
	rm -f a.out


cp:
	cp ../test2/test15.c ../test2-app/case.c
#----------------------------


test: ${TEST_CASE}
	@echo "refresh case"

# https://stackoverflow.com/questions/10475040/gcc-g-vs-g3-gdb-flag-what-is-the-difference
%.bc: %.c
	clang -c -emit-llvm -O0 -g3 $< -o $@
	# clang -c -emit-llvm -O0 $< -o $@

ra: ${RUN_DEP}
	 @for test in ${TEST_CASE}; do \
        echo $$test ; \
				./a.out $$test;\
				echo "";\
   done
