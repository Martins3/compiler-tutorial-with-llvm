FLAGS=-lclangBasic -lclangAST -lclangFrontend -lclangTooling -lLLVM -lclangSerialization -Wall
# FLAGS=$(llvm-config --cxxflags --ldflags --libs)

TEST_SRC = $(wildcard ../test3/*.c ../test3-app/*.c)
TEST_CASE = ${TEST_SRC:.c=.bc} 
RUN_DEP=${TEST_CASE} a.out

DEBUG_ME=../test3-app/case.bc

a.out:LLVMAssignment.cpp  Dataflow.h Liveness.h
	clang++ -g LLVMAssignment.cpp $(FLAGS)

#----------------------------
run: ${RUN_DEP}
	./a.out ${DEBUG_ME}

debug: ${RUN_DEP}
	gdb ./a.out -x gdbinit

clean:
	rm -f ${TEST_CASE}
	rm -f a.out


test: ${TEST_CASE}
	@echo "refresh case"

# https://stackoverflow.com/questions/10475040/gcc-g-vs-g3-gdb-flag-what-is-the-difference
%.bc: %.c
	clang -c -emit-llvm -O0 -g3 -Xclang -disable-O0-optnone $< -o $@
	# clang -c -emit-llvm -O0 $< -o $@

ra: ${RUN_DEP}
	 @for test in ${TEST_CASE}; do \
        echo $$test ; \
				./a.out $$test;\
				echo "";\
   done
