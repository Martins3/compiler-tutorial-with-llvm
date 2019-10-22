//===- Hello.cpp - Example code from "Writing an LLVM Pass" ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements two versions of the LLVM "Hello World" pass described
// in docs/WritingAnLLVMPass.html
//
//===----------------------------------------------------------------------===//
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Utils.h>

#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/DebugLoc.h>

#include "iostream"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/IR/Instructions.h>

using namespace llvm;

inline void TODO() {
  fprintf(stderr, "%s\n", "todo !");
  assert(false);
}

///!TODO TO BE COMPLETED BY YOU FOR ASSIGNMENT 2
struct FuncPtrPass : public FunctionPass {
  static char ID; // Pass identification, replacement for typeid

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    AU.addRequired<LoopInfoWrapperPass>();
  }

  void print_use_list(Instruction *inst) {

    for (Use &U : inst->operands()) {
      Value *v = U.get();
      if (dyn_cast<Instruction>(v)) {
        errs() << "\"" << *dyn_cast<Instruction>(v) << "\""
               << " -> "
               << "\"" << *inst << "\""
               << ";\n";
      }
      // 在此处抽出全局函数来 !
      if (v->getName() != "") {
        errs() << "\"" << v->getName() << "\""
               << " -> "
               << "\"" << *inst << "\""
               << ";\n";
        errs() << "\"" << v->getName() << "\""
               << " [ color = red ]\n";
      }
    }
  }

  void trace(Instruction *inst) {
    errs() << "trace begin :------>\n";
    // 如果是call 指令，只用知道第一条操作数
    // 我使用过谁 ? call 指令中间的参数发
    // 分别处理 trace load store 和 parameter passing, return value

    if (isa<CallInst>(&(*inst)) || isa<InvokeInst>(&(*inst))) {
      CallInst *t = cast<CallInst>(&(*inst));

      // value 类型包含 instruction
      auto f = t->getCalledOperand();
      if (isa<Instruction>(f)) {
        if (isa<LoadInst>(f)) {
          errs() << *f << "\n";
          trace(cast<Instruction>(f));
        } else {
          TODO();
        }
        // 猜测依赖的函数指针总是首先需要load一下
        // 借助一下global value 的力量
      } else {
        TODO();
      }

      // f->dump();
      goto trace_end;
    }

    if (isa<LoadInst>(&(*inst))) {
      LoadInst *t = cast<LoadInst>(&(*inst));
      // 找到从哪一个节点中间load 进去的，找到所有对其复制的位置即可 !
      // 含有推测？
      // 难道首先在函数中间对于store 的时候建立管理？
      auto v = t->getPointerOperand();
      // 似乎一定是 alloca 指令

      if (isa<Instruction>(v)) {
        auto i = cast<Instruction>(v);
        if (isa<AllocaInst>(i)) {
          auto a = cast<AllocaInst>(i);
          errs() << *a << "\n";

          trace(a);
        } else {
          TODO();
        }

      } else {
        TODO();
      }

      goto trace_end;
    }

    print_use_list(inst);

  trace_end:
    errs() << "<------ trace end\n";
  }

  FuncPtrPass() : FunctionPass(ID) {}
  virtual bool runOnFunction(Function &F) override {
    // 似乎递归就是被划分为四个层次的 !
    LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
    errs().write_escaped(F.getName());
    errs() << " : ";
    Module *M = F.getParent();
    CallGraph cg = CallGraph((*M));
    cg.dump(); // this is correct. It is printing the expected the call graph

    for (CallGraph::const_iterator itr = cg.begin(), ie = cg.end(); itr != ie;
         itr++) {
      if (itr->second != nullptr) {
        itr->second->dump();
        errs() << "-----------CGN---------\n";
        CallGraphNode *cgn = itr->second.get();

        if (const Function *fptr = cgn->getFunction()) {
          errs() << "Number of references are" << cgn->getNumReferences()
                 << "\n";
          errs() << fptr->getName() << "\n";

          if (cgn->operator[](0) != nullptr) {
            if (cgn->operator[](0)->getFunction() != nullptr) {
              errs() << cgn->operator[](0)->getFunction()->getName() << "\n";
            }
          }
        }
      }
    }

    errs() << F << "\n";

    // 函数指针递归处理
    errs() << "digraph " + F.getName() + "{\n";
    errs() << "\n";
    for (auto block = F.getBasicBlockList().begin();
         block != F.getBasicBlockList().end(); block++) {

      errs() << "block : " << *block << "\n";
      for (auto it = pred_begin(&*block), et = pred_end(&*block); it != et;
           ++it) {
        BasicBlock *predecessor = *it;
        errs() << "pre : " << *predecessor << "\n";
      }

      // 赋值节点
      // 1. 对于函数指针，可以非常简单的找到最开始的变量的内容
      // 2. 分析的基础是basic block ! 只要是函数指针，最后使用一定是从stack
      // 中间加载的
      // 3. 只要找到对于其赋值的位置即可 !
      // 4. 分析函数的basic block 找到其中的有效赋值
      //
      // llvm 连最基本的rename 都没有帮忙我们处理 !
      // 1. 只要访问局部变量就需要load 特定的位置的内容
      //
      // 1. 找到其中关于其中那些，没有赋值的继续，有赋值向上。
      // 2. 如果在当前block 中间直接捕获，那么直接GG
      //
      //
      // 3. 函数参数如何处理 ?

      errs();

      for (auto inst = block->begin(); inst != block->end(); inst++) {
        // 对于每一个instruction 查询功能
        errs() << "inst -----------> " << *inst << "\n";
        if (isa<CallInst>(&(*inst)) || isa<InvokeInst>(&(*inst))) {
          CallInst *t = cast<CallInst>(&(*inst));
          // TODO getFunction() 居然是获取当前的functionf
          if (auto f = t->getCalledFunction()) {
            // 似乎必须添加-g
            if (f->getName() != "llvm.dbg.declare") {
              errs() << "emmmmmmmmmm\t";
              // get name ?
              auto debug = inst->getDebugLoc();
              // if (debug.isImplicitCode()) {
              // errs() << "implicit ";
              // }else{
              // errs() << "not implicit ";
              // }
              errs() << debug.getLine() << " : ";
              errs() << f->getName() << "\n";
            }
          } else {
            trace(&(*inst));
          }
        } else {
          continue;
          // 不是所有指令都是含有 operands 依赖的
          for (Use &U : inst->operands()) {
            Value *v = U.get();
            if (dyn_cast<Instruction>(v)) {
              errs() << "\"" << *dyn_cast<Instruction>(v) << "\""
                     << " -> "
                     << "\"" << *inst << "\""
                     << ";\n";
            }
            // 无力处理函数参数
            if (v->getName() != "") {
              errs() << "\"" << v->getName() << "\""
                     << " -> "
                     << "\"" << *inst << "\""
                     << ";\n";
              errs() << "\"" << v->getName() << "\""
                     << " [ color = red ]\n";
            }
          }
        }
      }
    }
    errs() << "\n}\n";
    return false;

    // taken的含义是什么? 使用过而已，但是并不知道是否使用!
    errs() << "has address taken()"
           << (F.hasAddressTaken() == true ? "true" : "false") << "\n";
    // 获取函数的 entry block ?
    F.getEntryBlock();

    // F.dump();

    // https://stackoverflow.com/questions/43648780/get-filename-and-location-from-function
    // SmallVector<std::pair<unsigned, MDNode *>, 4> MDs;
    // F.getAllMetadata(MDs);
    // for (auto &MD : MDs) {
    // if (MDNode *N = MD.second) {
    // if (auto *subProgram = dyn_cast<DISubprogram>(N)) {
    // errs() << subProgram->getLine();
    // }
    // }
    // }
    //

    // 表现能力有限，无法处理指针 !
    // 应该在forloop 中间首先查询到
    // 1. 函数中间对于ins 中间循环 ins 然后call invoke 打印当前行号
    // 2. 获取函数
    errs() << "Let's find the usage:\n";
    for (User *U : F.users()) {
      if (Instruction *Inst = dyn_cast<Instruction>(U)) {
        errs() << "F is used in instruction:\n";
        errs() << *Inst << "\n";
      }
    }
    errs() << "Find over\n";
    return false;

    // 处理所有的指针
    for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
      errs() << "I:" << *I << "\n";

      // 输出内容不对啊，自身打印 ?
      for (Value::use_iterator i = I->use_begin(), ie = I->use_end(); i != ie;
           ++i) {
        // Value *v = *i;
        Instruction *instruction = dyn_cast<Instruction>(&*i);
        errs() << "ggggggggggg \t\t" << *instruction << "\n";
      }

      auto mm = I.getInstructionIterator();
      errs() << "mm:" << *mm << "\n";

      if (DILocation *Loc = I->getDebugLoc()) { // Here I is an LLVM instruction
        errs() << "debug llvm \n";
        // unsigned Line = Loc->getLine();
        // StringRef File = Loc->getFilename();
        // StringRef Dir = Loc->getDirectory();
        // bool ImplicitCode = Loc->isImplicitCode();
      } else {
        errs() << "debug failed \n";
      }

      // 忽然意识到, 似乎implicit 是没有location 的
      const DebugLoc &debug = I->getDebugLoc();
      if (debug.isImplicitCode()) {
        continue;
      }
      errs() << "debug : " << debug.getFnDebugLoc() << "\n";
      errs() << "debug : " << debug.getLine() << "\n";

      for (Use &U : I->operands()) {
        Value *v = U.get();
        errs() << "v:" << *v << "\n";

        Instruction *instruction = dyn_cast<Instruction>(v);
        if (instruction == nullptr) {
          continue;
        } else {
          const llvm::DebugLoc &debugInfo = instruction->getDebugLoc();

          std::string directory = debugInfo->getDirectory();
          std::string filePath = debugInfo->getFilename();
          int line = debugInfo->getLine();
          int column = debugInfo->getColumn();
          errs() << directory << "\t" << filePath << "\t" << line << "\t"
                 << column << "\n";
        }
      }
    }
    return false;

    // auto x = F.getSubprogram();

    for (Function::iterator b = F.begin(), be = F.end(); b != be; ++b) {
      errs() << "\n\t BB : ";
      bool isLoop = LI.getLoopFor(&*b);
      if (isLoop) {
        errs() << "loop{";
      }
      for (BasicBlock::iterator i = b->begin(), ie = b->end(); i != ie; ++i) {
        if (isa<CallInst>(&(*i)) || isa<InvokeInst>(&(*i))) {
          CallInst *t = cast<CallInst>(&(*i));
          // TODO CallInst
          if (t == nullptr) {
            errs() << "we can't get the function\n";
          } else {
            // t->dump();
            Function *f = t->getCalledFunction();
            if (f == nullptr) {
              errs() << "WTF !\n";
            } else {
              errs() << cast<CallInst>(&(*i))->getCalledFunction()->getName()
                     << "\t";
            }
          }
        }
      }
      if (isLoop) {
        errs() << "}";
      }
    }
    errs() << '\n';
    return false;
  }
};

char FuncPtrPass::ID = 0;
static RegisterPass<FuncPtrPass> X("funcptrpass",
                                   "Print function call instruction");

struct BBinLoops : public FunctionPass {
  static char ID; // Pass identification, replacement for typeid

  std::map<std::string, int> opCounter;
  BBinLoops() : FunctionPass(ID){};
  void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<LoopInfoWrapperPass>();
    AU.setPreservesAll();
  }

  void another(Function &F) {

    errs() << "Function " << F.getName() << '\n';

    for (Function::iterator bb = F.begin(), e = F.end(); bb != e; ++bb) {
      for (BasicBlock::iterator i = bb->begin(), e = bb->end(); i != e; ++i) {
        if (opCounter.find(i->getOpcodeName()) == opCounter.end()) {
          opCounter[i->getOpcodeName()] = 1;
        } else {
          opCounter[i->getOpcodeName()] += 1;
        }
      }
    }
    std::map<std::string, int>::iterator i = opCounter.begin();
    std::map<std::string, int>::iterator e = opCounter.end();
    while (i != e) {
      errs() << i->first << ": " << i->second << "\n";
      i++;
    }
    errs() << "\n";
    opCounter.clear();
  }
  virtual bool runOnFunction(Function &F) {

    LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
    int loopCounter = 0;
    errs() << F.getName() + "\n";
    // 获取loopInfo
    for (LoopInfo::iterator i = LI.begin(), e = LI.end(); i != e; ++i) {
      Loop *L = *i;
      int bbCounter = 0;
      loopCounter++;
      for (Loop::block_iterator bb = L->block_begin(); bb != L->block_end();
           ++bb) {
        bbCounter += 1;
      }
      errs() << "Loop ";
      errs() << loopCounter;
      errs() << ": #BBs = ";
      errs() << bbCounter;
      errs() << "\n";
    }
    return false;
  }
};

char BBinLoops::ID = 0;
static RegisterPass<BBinLoops> BB("bbloop",
                                  "Count the number of BBs inside each loop");

struct CFG : public FunctionPass {
  static char ID; // Pass identification, replacement for typeid
  CFG() : FunctionPass(ID) { errs() << "CFG"; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    AU.addRequired<LoopInfoWrapperPass>();
  }
  bool runOnFunction(Function &F) override {
    LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
    errs().write_escaped(F.getName());
    errs() << " : ";
    for (Function::iterator b = F.begin(), be = F.end(); b != be; ++b) {
      errs() << "\n\t BB : ";

      // TODO 根本无法理解使用，划分BB的原则是什么？
      bool isLoop = LI.getLoopFor(&*b);
      if (isLoop) {
        errs() << "loop{";
      }
      for (BasicBlock::iterator i = b->begin(), ie = b->end(); i != ie; ++i) {
        if (isa<CallInst>(&(*i)) || isa<InvokeInst>(&(*i))) {
          CallInst *t = cast<CallInst>(&(*i));
          // TODO CallInst
          if (t == nullptr) {
            errs() << "we can't get the function\n";
          } else {
            // t->dump();
            Function *f = t->getCalledFunction();
            if (f == nullptr) {
              errs() << "WTF !\n";
            } else {
              errs() << cast<CallInst>(&(*i))->getCalledFunction()->getName()
                     << "\t";
            }
          }
        }
      }
      if (isLoop) {
        errs() << "}";
      }
    }
    errs() << '\n';
    return false;
  }
};

char CFG::ID = 0;
static RegisterPass<CFG> C("CFG", "Gen CFG", true, true);

static cl::opt<std::string>
    InputFilename(cl::Positional, cl::desc("<filename>.bc"), cl::init(""));

int main(int argc, char **argv) {
  // https://stackoverflow.com/questions/41760481/what-should-i-replace-getglobalcontext-with-in-llvm-3-9-1
  static LLVMContext Context;
  SMDiagnostic Err;
  // Parse the command line to read the Inputfilename
  cl::ParseCommandLineOptions(
      argc, argv, "FuncPtrPass \n My first LLVM too which does not do much.\n");

  // Load the input module
  std::unique_ptr<Module> M = parseIRFile(InputFilename, Err, Context);
  if (!M) {
    Err.print(argv[0], errs());
    return 1;
  }

  llvm::legacy::PassManager Passes;

  /// Transform it to SSA
  Passes.add(llvm::createPromoteMemoryToRegisterPass());

  /// Your pass to print Function and Call Instructions
  Passes.add(new LoopInfoWrapperPass());
  Passes.add(new FuncPtrPass());
  // Passes.add(new BBinLoops());
  // Passes.add(new CFG());
  Passes.run(*M.get());
}
