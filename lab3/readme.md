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

## Read the test case
11. should be ok :
18. 
24. malloc `*mf_ptr` should handle specially(maybe never tested)


## TODO
1. remove the super stupid FuncOrPtr to Value 
2. not find & empty  : clear set  model should be rebuild, or we can't avoid the bug



3. create pointer, malloc and array  test case
  1. get the inst
  3. transter to 


All the time, we are handling the same problem,
what if one of some "crash", the answer is clear :
after this instruction, the dfval is cleaned !
1. generally, the block is called with correct data flow, 
but the algorithm works even the dataflow is random,
by clean the dfval
