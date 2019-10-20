#!/usr/bin/env bash


set -e
BUILD=/run/media/shen/Will/Compiler/bk/lab1
TEST=/run/media/shen/Will/Compiler/bk/test1
TEST_AP=/run/media/shen/Will/Compiler/bk/test1-app

EXE=$BUILD/a.out
cd $BUILD

if [ "$#" -eq 1 ]; then
  make
fi

if [ $1 == "all" ]; then
  for i in $TEST/*;do
    $EXE "`cat $i`"
    echo " "
    
    if [ $? -ne 0 ]; then
      echo "fail at $i"
    fi
  done

  for i in $TEST_AP/*;do
    $EXE "`cat $i`"
    echo " "
    
    if [ $? -ne 0 ]; then
      echo "fail at $i"
    fi
  done

else
  # ./a.out "`cat $TEST_AP/recursion.c`"
  ./a.out "`cat $TEST_AP/char.c`"
fi
