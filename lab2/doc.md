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
3. bonus 的情况处理一下，感觉无穷无尽
