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





# TODO
1. bc 格式  中间表示 ?
2. 获取函数名称 行号 ?
3. block 的划分依据是什么啊 ?
4. CallInst 和 InvokeInst 的区别是什么？
5. def-use chain   Value类型中间Use

1. 01 如果知道def-use chain，test02 的赋值是唯一的，根本不用思考的。
2. 02 列出plus 的所有可能即可 !
3. 03 同上
4. 04 基于函数参数的def use chain
5. 05 nothing new
6. 06 去掉明显不科学的数值
7. 07 相信llvm 的def use chain 可能的
8. 08 

> 测试样例没有处理 bonus 的情况!

函数指针不会实行increment 的，只有赋值运算，那么

# 理论补充
1. 构造调用图
2. 为什么需要构造出来dominance frontier 来 ?
    1. 计算应该很简单，大约划分为两种

3. 重命名的过程：
    1. 首先插入 fi (感觉没有什么困难的)
    2. 再也不需要的内容了 !

# 函数指针带来的挑战

如果出现对于af_ptr 的多次条件赋值
1. 当前函数指针被赋值给?
2. 计算当前函数哪一个GlobalVar 中间ref 过 ?
3. 总是自下向上的 ?
   1. function need premiter ? => map
   2. 分析调用的时候:
       1. 询问当前函数指针可能是什么，如果别人没有使用，那不是非常的尴尬?
       2. 对于参数需要赋值，只要检查参数的类型是fp 那么就是需要提供赋值的，建立一个表格!


4. 如果我们可以建立函数调用流程图
  1. 就算是建立了函数流程图，那么又怎么样啊 ?


5. https://llvm.org/doxygen/classllvm_1_1CallGraph.html
    1. callGraph 中间提供三个 CallGraphNode 返回调用过的函数和自身
    2. 

6. phi 检查一下内置的
https://llvm.org/doxygen/classllvm_1_1PHINode.html

7. phi 需要O3 观望一下!

callGraph 似乎只是普通的ref 内容，
希望可以提供那个函数ref 过，或者是哪一个指令ref过

为什么调用流程图类似:
1. 如果你知道你调用过该函数，那么一条调用指令确定了该
函数的使用啊!

# 分析一波函数
1. 处理掉直接调用 和 收集函数指针变量
2. 对于过程变量进行传播。



# todo
2. 基于Instruction 导致事情难以操作，参数，指令，指令中间的operand都是value
value 中间本身就是持有use list 的啊!

1. 向上查找store 到的value是什么东西?

2. map< call Instruction, class < parameter, return value, Function * >

3. 当前模型的基础:
    1. callInst get local store !
    2. 对于函数指针指令，对于函数指针和指针参数令分别向上的分析，


4. 存在递归调用的情况，无法求解。
5. 处理的方法: 只要内容稳定就停止
```c
typedef void(*P)();

void a(P p);
void b(P p){
  a(p);
}


void a(P p){
  p();
  b(p);
}

// 递归的含义，自己调用自己情况 !
```


```cpp
    // Module *M = F.getParent();
    // CallGraph cg = CallGraph((*M));
    // F.dump();
    // cg.dump(); // this is correct. It is printing the expected the call graph

    for (CallGraph::const_iterator itr = cg.begin(), ie = cg.end(); itr != ie;
         itr++) {

      if (itr->first != nullptr) {
        itr->first->dump();
      } else {
        errs() << "function is null, we don't know why\n";
      }

      if (itr->second != nullptr) {
        errs() << "----------dump Graph node---\n";
        itr->second->dump();
        errs() << "-----------CGN---------\n";
        CallGraphNode *cgn = itr->second.get();

        if (const Function *fptr = cgn->getFunction()) {
          errs() << "Number of references are" << cgn->getNumReferences()
                 << "\n";
          errs() << "My name : " << fptr->getName() << "\n";
          // refNum 并不是数值，而仅仅对于名称的使用!

          // 可以查出来使用哪一个函数，如果直接找到其中的instruction就好了
          errs() << "**********\n";
          for (auto ref = cgn->begin(); ref != cgn->end(); ref++) {
            ref->second->dump();
          }
          errs() << "**********\n";


          // 描述的ref 的函数
          errs() << cgn->size() << "\n";
          errs() << cgn->empty() << "\n";

          // called by this node
          // TODO 首先检查一下是否调用过函数

          errs() << "List function ref by me : \n";
          if (auto f = cgn->size() > 0) {
            for (int i = 0; i < f; ++i) {
              if (auto node = cgn->operator[](i)) {
                node->dump();
                if (node->getFunction() != nullptr) {
                  errs() << cgn->operator[](0)->getFunction()->getName()
                         << "\n";
                  auto f = cgn->operator[](0)->getFunction();
                  f->dump();
                }
              }
            }
          }
          errs() << "List function ref by me end\n";
        }
      }
    }
```

```cpp
// 测试显示，如果不添加-g 根本没有办法获取行号
          if(auto debug = inst->getDebugLoc()){
            if(debug.isImplicitCode()){
                errs() << debug.getLine() << " : ";
            }else{
                errs() << "not implicit ";
                TODO();
            }
          }
```
