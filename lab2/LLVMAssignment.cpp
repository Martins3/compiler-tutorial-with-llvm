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
#include <set>

using namespace llvm;

inline void TODO() {
  fprintf(stderr, "%s\n", "todo !");
  assert(false);
}

// Origin 是一个节点!
struct Origin {
  Function *F; // 当前Origin 对对应的函数
  Origin(Function *f) : F(f){};

  void print() {
    errs() << "debug origin ************* \n";
    errs() << "current function : " << F->getName() << "\n";

    for (auto f : fun) {
      errs() << f->getName() << "\t";
    }

    for (auto f : para) {
      errs() << f << "\t";
    }

    // TODO 可能出现本来含有递归!
    for (auto o : ret) {
      o->print();
    }

    errs() << "\ndebug origin end *********\n";
  }

  std::set<Function *> fun; // 直接函数赋值传递过来的

  // Origin 需要和具体函数挂钩!
  std::set<unsigned int> para; // 多个函数参数
  // 需要进一步调用 : call指令给参数的trace !

  std::set<Origin *> ret; // 多个函数的ret value !
  // 表示为这些来源的返回值 !
  // %3 = call plus
  // %3 = call plus
  bool pending() { return para.size() > 0 || ret.size() > 0; }
};

void trace_store(Origin *ori, BasicBlock *block, LoadInst *v);

// 由函数定义形成的连接关系
std::map<Function *, Origin *> function_ret;

// inv 形成的bridge
std::map<Origin *, std::vector<Origin *>> para_pas;
// 如果不是fptr，为nullptr

// 所有的节点 : 返回值 函数 和
std::set<Origin *> ori_nodes;

// call/invoke 指令 和 其对应的Origin
std::map<Instruction *, Origin *> result;

Value *this_block_store_me(BasicBlock *b, LoadInst *v) {

  auto contain_me = false;
  for (auto inst = b->rbegin(); inst != b->rend(); inst++) {
    if (v == &*inst) {
      contain_me = true;
      break;
    }
  }

  for (auto inst = b->rbegin(); inst != b->rend(); inst++) {
    if (contain_me && v != &*inst) {
      continue;
    } else {
      contain_me = false;
    }

    if (auto store = dyn_cast<StoreInst>(&*inst)) {
      // Value * 	getValueOperand ()
      // Value * 	getPointerOperand ()
      if (store->getPointerOperand() == v->getPointerOperand()) {
        // 直接返回ValueOperand，是一个什么样子都可以的数值!
        return store->getValueOperand();
      }
    }
  }

  return nullptr;
}

LoadInst *get_alloc(Origin *ori, Value *v) {
  if (auto load = dyn_cast<LoadInst>(v)) {
    return load;
  }
  if (auto f = dyn_cast<Function>(v)) {
    ori->fun.insert(f);
  } else if (auto arg = dyn_cast<Argument>(v)) {
    ori->para.insert(arg->getArgNo());
  } else if (auto call = dyn_cast<CallInst>(v)) {
    // call 是函数的返回值!
    auto called = call->getCalledOperand();
    auto call_ori = new Origin(call->getParent()->getParent());
    if (auto load = get_alloc(call_ori, called)) {
      trace_store(call_ori, load->getParent(), load);
    }
    ori->ret.insert(call_ori);
  } else if (isa<ConstantPointerNull>(v)) {
    // TODO 并没有办法处理(FP)12 之类的情况!
    // TODO bitcast 无力处理!
    errs() << "maybe call nullptr\n";
  } else {
    errs() << *v << "\t";
    errs() << "Holy shit\n";
    assert(false);
  }

  return nullptr;
}

/**
 * 检查在该loadInst 之前一共含有的store 的可能!
 * strace store for the loadInst
 */
void trace_store(Origin *ori, BasicBlock *block, LoadInst *v) {
  // get load instruction
  // 如果是load 指令，得到其store指令
  // 从当前的load 指令向上store 指令，然后对于store 进行make_Origin 操作
  assert(block != nullptr && v != nullptr);

  if (auto sv = this_block_store_me(block, v)) {
    if (auto load = get_alloc(ori, sv)) {
      // auto load = dyn_cast<LoadInst>(v);
      assert(load != nullptr);
      trace_store(ori, block, load);
    }
  } else {
    for (auto it = pred_begin(&*block), et = pred_end(&*block); it != et;
         ++it) {
      trace_store(ori, *it, v);
    }
  }
}

// 外部接口
void make_Origin(Origin *ori, BasicBlock *block, Value *v) {
  assert(block != nullptr && v != nullptr);
  if (auto load = get_alloc(ori, v)) {
    assert(load != nullptr);
    trace_store(ori, block, load);
  }
}

void collect_nodes(Instruction *inst) {
  // graph 的连接 : invoke 指令 和 ret 指令

  auto F = inst->getParent()->getParent();
  // is call invoke or ret
  if (isa<CallInst>(&(*inst)) || isa<InvokeInst>(&(*inst))) {
    CallInst *t = cast<CallInst>(&(*inst));
    // 无论call 是否使用函数指针，都需要制作其Origin，除非参数中间没有fptr
    // 还是对于其mkptr 因为第二次扫描的过程中间，通过遍历nodes 查找，更加简单

    Origin *called_ori = new Origin(F);
    auto call = t->getCalledOperand();
    make_Origin(called_ori, t->getParent(), call);
    result[t] = called_ori;
    ori_nodes.insert(called_ori);

    std::vector<Origin *> para_vec;
    for (int i = 0; i < t->getNumArgOperands(); ++i) {
      auto arg = t->getArgOperand(i);
      auto ty = arg->getType();
      Origin *ori = nullptr;

      if (ty->isPointerTy()) {
        if (ty->getPointerElementType()->isFunctionTy()) {
          ori = new Origin(F);
          make_Origin(ori, t->getParent(), arg);
          ori_nodes.insert(ori);
        }
      }
      para_vec.push_back(ori);
    }
    para_pas[called_ori] = para_vec;


  } else if (isa<ReturnInst>(&*inst)) {
    ReturnInst *t = cast<ReturnInst>(&*inst);

    auto retType = t->getReturnValue()->getType();
    if (retType->isPointerTy()) {
      if (retType->getPointerElementType()->isFunctionTy()) {
        Origin *ori = new Origin(F);
        make_Origin(ori, t->getParent(), t->getReturnValue());
        ori_nodes.insert(ori);
        function_ret[F] = ori;
      }
    }
  }
}

// 定义多个pass
// 1. 首先收集信息 上面两条内容

// 不要递归啊!
bool dig_me(Origin *ori) {

  auto size = ori->fun.size();
  if (ori->pending()) {
    for (auto index : ori->para) {
      auto f = ori->F;
      for (auto pas : para_pas) {
        if (pas.first->fun.find(f) != pas.first->fun.end()) {
          if (index > pas.second.size()) {
            // 插入函数，
            // errs() << *f << "\n";
            assert(false);
          }
          auto p = pas.second[index];
          ori->fun.insert(p->fun.begin(), p->fun.end());
        }
      }
    }

    for (auto caller : ori->ret) {
      for (auto f : caller->fun) {
        auto l = function_ret.find(f);
        if (l != function_ret.end()) {
          auto ret = l->second;
          ori->fun.insert(ret->fun.begin(), ret->fun.end());
        }
      }
    }
    // print the function name
    // TODO debug has changed !
    return size != ori->fun.size();
  }

  return false;
}

void expand_the_graph() {
  while (true) {
    bool change = false;
    for (auto o : ori_nodes) {
      if (dig_me(o)) {
        change = true;
      }
    }

    if (!change) {
      break;
    }
  }
}

///!TODO TO BE COMPLETED BY YOU FOR ASSIGNMENT 2
struct FuncPtrPass : public FunctionPass {
  static char ID; // Pass identification, replacement for typeid

  FuncPtrPass() : FunctionPass(ID) {}
  virtual bool runOnFunction(Function &F) override {
    errs() << F << "\n";
    for (auto block = F.getBasicBlockList().begin();
         block != F.getBasicBlockList().end(); block++) {
      for (auto inst = block->begin(); inst != block->end(); inst++) {
        collect_nodes(&*inst);
      }
    }
    return false;
  }

  virtual bool doFinalization(Module &M) override {
    expand_the_graph();
    for (auto F = M.begin(); F != M.end(); F++) {
      for (auto B = F->begin(); B != F->end(); B++) {
        for (auto inst = B->begin(); inst != B->end(); inst++) {
          if (isa<CallInst>(&(*inst)) || isa<InvokeInst>(&(*inst))) {
            CallInst *t = cast<CallInst>(&(*inst));
            Origin *ori = result[t];
            // pending 其实意义
            if (false && ori->pending()) {
              errs() << *inst << "\n";
              ori->print();

              errs() << "function ret : \n";
              for (auto ret : function_ret) {
                errs() << *ret.first << "\n";
                ret.second->print();
              }
              assert(false);
            }
            for (auto f : ori->fun) {
              errs() << f->getName() << "\t";
            }
            errs() << "\n";
          }
        }
      }
    }
    return false;
  }
};

char FuncPtrPass::ID = 0;
static RegisterPass<FuncPtrPass> X("funcptrpass",
                                   "Print function call instruction");

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
  Passes.add(new FuncPtrPass());
  Passes.run(*M.get());
}
