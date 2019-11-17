# links
https://www.inf.ed.ac.uk/teaching/courses/ct/17-18/slides/llvm-2-writing_pass.pdf
http://llvm.org/docs/WritingAnLLVMPass.html

# 处理forloop
1. 将block 组合成为所有的可能的情况，形成路径
2. forloop 如何处理
3. forloop 中间含有各种if while 和 forloop
4. forloop 仅仅假设分析一次即可 !?



# goal
作业2 （20分）
1.  目的： 熟悉LLVM中间表示，use-def链
2. 对bitcode文件中的函数调用指令
    1.  直接调用:  打印被调用函数的名称和行号 （5分）
    2.  函数指针
        1.  计算出可能调用的函数 （10分）
        2.  如果可能调用的函数确定，将其替换成直接调用(5分）
        3.  不考虑当函数指针被Store到memory中的情况
3. Bonus: 10分，考虑函数指针被Store到memory中的情况

# some doc
We go over LLVM data structures through iterators. 
• An iterator over a Module gives us a list of FuncAons. 
• An iterator over a Func@on gives us a list of basic blocks. 
• An iterator over a Block gives us a list of instrucAons. 
• And we can iterate over the operands of the instrucAon too.

https://stackoverflow.com/questions/30195204/how-to-parse-llvm-ir-line-by-line
https://stackoverflow.com/questions/54193082/how-to-get-the-original-source-and-line-number-in-llvm
https://stackoverflow.com/questions/18305965/get-c-c-source-code-data-from-llvm-ir

https://llvm.org/docs/FAQ.html


http://llvm.org/docs/ProgrammersManual.html
1. http://llvm.org/docs/ProgrammersManual.html#advanced-topics
2. http://llvm.org/docs/ProgrammersManual.html#the-core-llvm-class-hierarchy-reference
3. http://llvm.org/docs/ProgrammersManual.html#iterating-over-def-use-use-def-chains
> 提供了插入新的指令API

4. CallInst 似乎也提供了大量API来create 新的指令。

# goto
4. 函数指针的指针
5. 作为一个 field 
4. 指令的替换!
5. struct point my_point[42];


> 如果全部都是寄存器内容，那么 alloc 检查，def-use 
> ? 出现数字计算的 ? 每个偏移量保存为一个 reg

> 数组 : 最简单 : 记录长度访问 ! 
> 结构体 : 偏移的两个参数 ?
> 指针 : 有时候含有 ?

```c
%2 = getelementptr inbounds [10 x i32 (i32, i32)*], [10 x i32 (i32, i32)*]* %1, i64 0, i64 1
1. 两个类型
2. 两个参数
3. i32 和 i64


  %2 = getelementptr inbounds [10 x i32 (i32, i32)*], [10 x i32 (i32, i32)*]* %1, i64 0, i64 1
[10 x i32 (i32, i32)*]
i32 (i32, i32)*
  %1 = alloca [10 x i32 (i32, i32)*], align 16
2
i64 0
i64 1
```

对于数组建立表格，

指针访问，都会被变换为数组，然后在该结构体中间。

2. 只能持有结构体的指针，而不能持有结构体在reg中间。
3. 偏移量必须为常数。

常数是否支持唯一标志。

多条路径到达 ?
1. 不会的，只要不含有union的情况, 所以的单元互相没有交集

1. 在load 的过程中间的指针类型装换，导致偏移量为0的被忽视. 



难道导致整个体系重做。
1. 对于，load直接获取 alloc 的，依旧采用之前的
2. 否则分析对于根节点的 alloc
3. 然后向上扫描整个store由于不知道store的来源，导致所有的store都需要分析。
4. store load 指令的确会去掉 0 的操作。


既然都持有了整个的调用流程，那么为什么不采用，直接从上向下计算的方法，
对于每一个文件的变量的来源的计算一个图。
1. 因为当前的根本不能跟踪到制定的位置!

数组类型加上参数传递。


当前的工作，将代码整理到体系中间，5点结束 ! 提交的那种!
