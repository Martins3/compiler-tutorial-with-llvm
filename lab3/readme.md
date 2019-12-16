# Wow
You are supposed to implement a flow-sensitive, field- and context-insensitive algorithm to compute the 
points-to set for each variable at each distinct program point.
One approach is to extend the provided dataflow analysis framework as follows: 
- Implement your representation of points-to sets
- Implement the transfer function for each instruction
- Implement the MEET operator
- Extend the control flow graph to be inter-procedural




## Read the code framework
liveness 的分析 : flow sensitive 就是succ 和 sub 得到的

1. anderson 算法的内容是什么 ?
2. 四种种指针的关系 ： 导致的操作
  1. 取地址 : 更新指向集合
  2. must point to & may point to
  3. alias analysis & point-to analysis
  4. Ignoring for the moment memory allocation and arrays, we can decompose all pointer operations into four instruction types: taking the address of a variable, copying a pointer from one variable to another, assigning through a pointer, and dereferencing a pointer

3. 处理malloc 数组的方法 ?

4. 如何实现 flow sensitive ? 
> 将所有的

https://www.cs.cmu.edu/~aldrich/courses/15-819O-13sp/resources/pointer.pdf
https://www.cs.utexas.edu/users/lin/papers/cgo11.pdf

1. 如何处理多重指针 ? (多种指针还是指针)
2. interprocedural 函数参数传递如何进行 ?
3. 如果指向集合导致条状的函数不确定 ? (全部分析一下)(那么持有确定的call graph 了，)
4. 由于是 context-insensitive 的，导致对于所有的函数。
    1. 当一个节点的 Info 发生改变了，在函数的入口引入关系，适当做什么内容处理 ? (创建新的指向集合，还是直接复用 ?(利用assign 吧))
    2. compBackwardDataflow 只能用于函数。
      1. 其实，利用compDFVal 函数进行分析，当遇到每一个函数的时候，进入到函数中间，然后对于该进行 compBackwardDataflow , 其中 initval 通过参数分析得到 !
      2. 

我TM 的 phi 节点在哪里啊 ?



## Read the test case
alias 问题，如何知道 ?
1. 函数就是int (treat it as, 当使用，对于foo minus 之类的, 遇到的时候，处理为 ...)
2. 数组的处理
3. field 处理 :
    1. 结构体本身为指针，x.a = ptr 相当于 x 的指向集合中间含 ptr
    2. 当 `x->a`，相当于进行两次 `*` 操作

## TODO
1. remove the super stupid FuncOrPtr to Value 
2. not find & empty  : clear set  model should be rebuild, or we can't avoid the bug


3. create pointer, malloc and array  test case
  1. get the inst
  3. transter to 
  
