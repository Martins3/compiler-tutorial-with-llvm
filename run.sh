#!/usr/bin/env bash

set -e
BUILD=/run/media/shen/Will/Compiler/llvm-project/build
TEST=/run/media/shen/Will/Compiler/bk/test1

EXE=$BUILD/bin/lab1
cd $BUILD
if [ "$#" -eq 1 ]; then
  ninja
  # ninja -t compdb cxx > compile_commands.json
fi

if [ $1 == "all" ]; then
  for i in $TEST/*;do
    # cat $i
    $EXE "`cat $i`"
    echo " "
    
    if [ $? -ne 0 ]; then
      echo "fail at $i"
    fi

  done
else
$EXE "`cat $TEST/test$1.c`"
fi



