# Build your own compiler with llvm and clang


## Setup
1. git clone https://github.com/llvm/llvm-project
2. git clone `this repo`
3. cd ./llvm-project/clang-tools-extra && mkdir lab1
4. link every file from lab1 to ./llvm-project/clang-tools-extra/lab1 (maybe I will write a shell)
5. setup run.sh(change variable `BUILD` and `TEST` to your )
6. ./run.sh and goto sleep !

attention:
1. hard disk 100G required
2. memory plus swap 20G+ required, as for how to setup swap, refer [this](https://linuxize.com/post/create-a-linux-swap-file/).
3. compiling c++ is unbearably slow.

## Before the first line of code !
1. check [visitor pattern](https://en.wikipedia.org/wiki/Visitor_pattern)


## Hints
1. What's the difference between `Visit` and `VisitStmt` ?
