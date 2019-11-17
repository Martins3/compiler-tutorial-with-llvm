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

#define PERMU_BLOCK 1

#ifndef PERMU_BLOCK
#error "In fact, current algorithm can't handle back-edge, forloop!"
#endif

inline void TODO() {
  fprintf(stderr, "%s\n", "todo !");
  assert(false);
}

// Origin 是一个节点!
class Origin {
public:
  Function *F; // 当前Origin 对对应的函数

  // FIXME 目前的机制其实仅仅处理函数内部的含有null, 但是如果null
  // 作为函数的参数传递其实没有被考虑的! 还不如直接剔除整个对于null的考虑
  // 来源来自于null 导致其必然不会参加edge 的构建
  bool hasConstantPointerNull;

  std::set<Function *> fun; // 直接函数赋值传递过来的

  // Origin 需要和具体函数挂钩!
  std::set<unsigned int> para; // 多个函数参数
  // 需要进一步调用 : call指令给参数的trace !

  std::set<Origin *> ret; // 多个函数的ret value !
  // 表示为这些来源的返回值 !
  // %3 = call plus
  // %3 = call plus
  bool pending() { return para.size() > 0 || ret.size() > 0; }

  Origin(Function *f) : F(f), hasConstantPointerNull(false){};

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
};

// 由函数定义形成的连接关系
std::map<Function *, Origin *> function_ret;

// inv 形成的bridge
std::map<Origin *, std::vector<Origin *>> para_pas;
// 如果不是fptr，为nullptr

// 所有的节点 : 返回值 函数 和
std::set<Origin *> ori_nodes;

// call/invoke 指令 和 其对应的Origin
// 如果使用PERMU_BLOCK 的时候，这只是一种收集的内容，
// 查询还是需要从final_result 中间
std::map<Instruction *, Origin *> result;

#ifdef PERMU_BLOCK
std::map<Instruction *, std::set<Function *>> final_result;
std::map<Function *, std::vector<std::vector<BasicBlock *>>> possible_seq;

// maybe we can have a better solution ?
void add_one_call_seq(Function *f, std::vector<BasicBlock *> &vec) {
  auto find = possible_seq.find(f);
  if (find != possible_seq.end()) {
    find->second.push_back(vec);
  } else {
    possible_seq[f] = std::vector<std::vector<BasicBlock *>>{vec};
  }
}

void clear_birdge() {
  function_ret.clear();
  para_pas.clear();
  ori_nodes.clear();
  result.clear();
}

bool cmpValue(Value *L, Value *R) {
  std::string bufferL;
  raw_string_ostream osL(bufferL);
  osL << *L;
  std::string strVL = osL.str();

  std::string bufferR;
  raw_string_ostream osR(bufferR);
  osR << *R;
  std::string strVR = osR.str();

  if (strVL != strVR)
    return false;

  return true;
}

bool cmpGetElementPtrInst(GetElementPtrInst *gep_left,
                          GetElementPtrInst *gep_right) {

  unsigned int ASL = gep_left->getPointerAddressSpace();
  unsigned int ASR = gep_right->getPointerAddressSpace();
  if (ASL != ASR)
    return false;

  Type *ETL = gep_left->getSourceElementType();
  Type *ETR = gep_right->getSourceElementType();

  // 比较类型一致
  std::string bufferL;
  raw_string_ostream osL(bufferL);
  osL << *ETL;
  std::string strETL = osL.str();

  std::string bufferR;
  raw_string_ostream osR(bufferR);
  osR << *ETR;
  std::string strETR = osR.str();

  if (strETL != strETR)
    return false;

  unsigned int NPL = gep_left->getNumOperands();
  unsigned int NPR = gep_right->getNumOperands();

  if (NPL != NPR)
    return false;

  // 在指针中间的偏移量一致
  // 可以处理其中变量为偏移量的情况的啊 !
  for (unsigned i = 0, e = gep_left->getNumOperands(); i != e; ++i) {
    Value *vL = gep_left->getOperand(i);
    Value *vR = gep_right->getOperand(i);
    if (cmpValue(vL, vR) == false)
      return false;
  }
  return true;
}

// 放到call 指令的路线中间
// 处理的情况太简单了
// 1. 不能处理多级嵌套
// 2. 不能处理来自于函数的参数和返回值传递
// 3. malloc 的内容也不能处理
void getGetElementPtrInst(GetElementPtrInst *getElementPtrInst, Origin * ori) {
  Value *v = getElementPtrInst->getOperand(0);
  for (User *U : v->users()) {
    // 如果数值来源于gep，那么gep对外的分发数据时钟为gep
    if (GetElementPtrInst *gepi = dyn_cast<GetElementPtrInst>(U)) {
      // 判断前者是不是store 到同一个位置 ?
      if (cmpGetElementPtrInst(getElementPtrInst, gepi) == true) {
        // 检查是不是访问到的位置
        for (User *UL : gepi->users()) {
          // 检查是不是store 指令，到同一个位置
          if (StoreInst *storeInst = dyn_cast<StoreInst>(UL)) {
            Value *vl = storeInst->getOperand(0);
            if (Function *func = dyn_cast<Function>(vl)) {
              ori->fun.insert(func);
            }
          }
        }
      }
    }
  }
}

// clear this one
void trace_store(Origin *ori, BasicBlock *block, LoadInst *v,
                 const std::vector<BasicBlock *> &vec);
#else
void trace_store(Origin *ori, BasicBlock *block, LoadInst *v);
#endif

/**
 * 1. 曾经的假设，当出现load, load 指向的内容必定是 alloc 的，而且 alloc
 * 出现的位置 在函数开始的 basicblock 中
 * 2.
 */
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

#ifdef PERMU_BLOCK
LoadInst *get_alloc(Origin *ori, Value *v,
                    const std::vector<BasicBlock *> &vec) {
#else
LoadInst *get_alloc(Origin *ori, Value *v) {
#endif
  // 在之前的模型中间，load 指令的操作数来源必定是第一个block的alloc指令的结果
  if (auto load = dyn_cast<LoadInst>(v)) {
    // load 来自于一个
    if (auto f = dyn_cast<GetElementPtrInst>(load->getOperand(0))) {
      getGetElementPtrInst(f, ori);
      return nullptr;
    } else {
      return load;
    }
  }

  if (auto f = dyn_cast<Function>(v)) {
    ori->fun.insert(f);
  } else if (auto arg = dyn_cast<Argument>(v)) {
    ori->para.insert(arg->getArgNo());
  } else if (auto call = dyn_cast<CallInst>(v)) {
    // call 是函数的返回值!
    auto called = call->getCalledOperand();
    auto call_ori = new Origin(call->getParent()->getParent());
#ifdef PERMU_BLOCK
    if (auto load = get_alloc(call_ori, called, vec)) {
      trace_store(call_ori, load->getParent(), load, vec);
#else
    if (auto load = get_alloc(call_ori, called)) {
      trace_store(call_ori, load->getParent(), load);
#endif
    }
    ori->ret.insert(call_ori);
  } else if (auto nu = dyn_cast<ConstantPointerNull>(v)) {
    ori->hasConstantPointerNull = true;
  } else {
    // TODO bitcast 无力处理，而且导致bitcast 的情况不只有这些!
    errs() << *v << "\t";
    errs() << "Holy shit, you should reach here !\n";
    assert(false);
  }

  return nullptr;
}

#ifdef PERMU_BLOCK
/**
 * 重写pre_begin的含义即可!
 */
void trace_store(Origin *ori, BasicBlock *block, LoadInst *v,
                 const std::vector<BasicBlock *> &vec) {
  assert(block != nullptr && v != nullptr);

  if (auto sv = this_block_store_me(block, v)) {
    // 如果获取 get_alloc 返回不为 nullptr 那么需要继续递归分析
    if (auto load = get_alloc(ori, sv, vec)) {
      assert(load != nullptr);
      trace_store(ori, block, load, vec);
    }
  }

  // this block doesn't store me, find the upper one !
  else {
    for (auto b = vec.rbegin(); b != vec.rend(); b++) {
      if (*b == block) {
        b++;
        if (b != vec.rend()) {
          trace_store(ori, *b, v, vec);
        } else {
          // 就是存在不确定的情况
          // 表示当前route没有对于该数值赋值
          // 查不出数值，问题也不是很严重
        }
        return;
      }
    }
    errs() << "检查工作在外部已经完成过了";
    assert(false);
  }
}
#else
/**
 * 检查在该loadInst 之前一共含有的store 的可能!
 * trace store for the loadInst
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
#endif

// 外部接口
void make_Origin(Origin *ori, BasicBlock *block, Value *v,
                 const std::vector<BasicBlock *> &vec) {
  assert(block != nullptr && v != nullptr);

#ifdef PERMU_BLOCK
  if (auto load = get_alloc(ori, v, vec)) {
    assert(load != nullptr);
    trace_store(ori, block, load, vec);
  }
#else
  if (auto load = get_alloc(ori, v)) {
    assert(load != nullptr);
    trace_store(ori, block, load);
  }
#endif
}

#ifdef PERMU_BLOCK
void collect_nodes(Instruction *inst, const std::vector<BasicBlock *> &vec) {
#else
void collect_nodes(Instruction *inst) {
#endif
  // graph 的连接 : invoke 指令 和 ret 指令

  auto F = inst->getParent()->getParent();
  // is call invoke or ret
  if (isa<CallInst>(&(*inst)) || isa<InvokeInst>(&(*inst))) {
    CallInst *t = cast<CallInst>(&(*inst));
    if (auto f = t->getCalledFunction()) {
      // llvm.dbg.declare
      if(f->isIntrinsic()){
        return;
      }
    }

    // 无论call 是否使用函数指针，都需要制作其Origin，除非参数中间没有fptr
    // 还是对于其mkptr 因为第二次扫描的过程中间，通过遍历nodes 查找，更加简单

    Origin *called_ori = new Origin(F);
    auto call = t->getCalledOperand();
#ifdef PERMU_BLOCK
    make_Origin(called_ori, t->getParent(), call, vec);
#else
    make_Origin(called_ori, t->getParent(), call);
#endif
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
#ifdef PERMU_BLOCK
          make_Origin(ori, t->getParent(), arg, vec);
#else
          make_Origin(ori, t->getParent(), arg);
#endif
          ori_nodes.insert(ori);
        }
      }
      para_vec.push_back(ori);
    }
    para_pas[called_ori] = para_vec;

  } else if (isa<ReturnInst>(&*inst)) {
    ReturnInst *t = cast<ReturnInst>(&*inst);
    if (t->getReturnValue() != nullptr) {
      auto retType = t->getReturnValue()->getType();
      if (retType->isPointerTy()) {
        if (retType->getPointerElementType()->isFunctionTy()) {
          Origin *ori = new Origin(F);
#ifdef PERMU_BLOCK
          make_Origin(ori, t->getParent(), t->getReturnValue(), vec);
#else
          make_Origin(ori, t->getParent(), t->getReturnValue());
#endif
          ori_nodes.insert(ori);
          function_ret[F] = ori;
        }
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

#ifdef PERMU_BLOCK
int get_succ_num(BasicBlock *block) {
  int num = 0;
  auto it = succ_begin(&*block);
  auto et = succ_end(&*block);
  for (; it != et; ++it) {
    num++;
  }
  return num;
}


void permutate_blocks(std::vector<BasicBlock *> &vec, BasicBlock *block) {
  vec.push_back(block);
  auto it = succ_begin(&*block);
  auto et = succ_end(&*block);
  if (it == et) {
    add_one_call_seq(block->getParent(), vec);
  } else {
    for (; it != et; ++it) {
      BasicBlock *succ = *it;
      bool find_do_while = false;
      for (BasicBlock *b : vec) {
        if (succ == b) {
          auto b_succ_num = get_succ_num(b);

          // do while 回指向直接无视，
          if (b_succ_num == 1) {
            find_do_while = true;
            break;
          }

          // for 和 while 回来指向分支为两条, 其中一条是我们需要的出口
          else if (b_succ_num == 2) {
            auto it = succ_begin(&*b);
            auto succ_a = *it;
            it++;
            auto succ_b = *it;

            for (auto v : vec) {
              if (v == succ_a) {
                succ_a = nullptr;
                break;
              }
            }

            for (auto v : vec) {
              if (v == succ_b) {
                succ_b = nullptr;
                break;
              }
            }

            BasicBlock *next;
            if (succ_a == nullptr) {
              assert(succ_b != nullptr);
              next = succ_b;
            }

            else if (succ_b == nullptr) {
              assert(succ_a != nullptr);
              next = succ_a;
            }

            else {
              errs() << "必须是一个访问过，一个没有访问过\n";
              assert(false);
            }

            /* vec.push_back(b); */
            permutate_blocks(vec, next);
            /* vec.pop_back(); */
            vec.pop_back();
            return;
          }

          else {
            errs() << "没有goto语句，仅仅分析if while 和 do while\n";
            assert(false);
          }
        }
      }
      if (find_do_while) {
        continue;
      }
      permutate_blocks(vec, succ);
    }
  }
  vec.pop_back();
}

void collect_nodes_in_one_func(std::vector<BasicBlock *> &vec) {
  for (auto block : vec) {
    for (auto inst = block->begin(); inst != block->end(); inst++) {
      // llvm-9 seems doesn't generate phi and select
      if (isa<PHINode>(inst)) {
        errs() << "we don't use phi !\n";
        assert(false);
      }
      collect_nodes(&*inst, vec);
    }
  }
}

void do_one_possible_route(
    std::map<Function *, std::vector<std::vector<BasicBlock *>>>::iterator i,
    std::vector<std::vector<BasicBlock *>> &vec) {
  if (i == possible_seq.end()) {
    for (auto p : vec) {
      collect_nodes_in_one_func(p);
    }

    expand_the_graph();
    for (auto res : result) {
      auto f = final_result.find(res.first);
      if (f == final_result.end()) {
        std::set<Function *> v;
        for (auto fun : res.second->fun) {
          v.insert(fun);
        }
        if (res.second->hasConstantPointerNull)
          v.insert(nullptr);

        final_result[res.first] = v;
      } else {
        for (auto fun : res.second->fun) {
          f->second.insert(fun);
        }
        if (res.second->hasConstantPointerNull)
          f->second.insert(nullptr);
      }
    }
    clear_birdge(); // 收割结束，桥梁重做
  } else {
    for (auto p : i->second) {
      // wired trick
      ++i;
      auto next_i = i;
      --i;
      /* collect_nodes_in_one_func(p); */
      vec.push_back(p);
      do_one_possible_route(next_i, vec);
      vec.pop_back();
    }
  }
}

void trace_all_possible_route() {
  if (possible_seq.size() == 0)
    return;
  auto f = possible_seq.begin();
  std::vector<std::vector<BasicBlock *>> vec;

  for (auto p : f->second) {
    vec.push_back(p);
    do_one_possible_route(f, vec);
    vec.pop_back();
  }

  assert(vec.size() == 0);
}
#endif

///!TODO TO BE COMPLETED BY YOU FOR ASSIGNMENT 2
struct FuncPtrPass : public FunctionPass {
  static char ID; // Pass identification, replacement for typeid

  FuncPtrPass() : FunctionPass(ID) {}
  virtual bool runOnFunction(Function &F) override {
    /* errs() << F << "\n"; */

    // 让其从下向上进行遍历，
    // 这一个部分完成origin 的建立和收集工作
#ifdef PERMU_BLOCK

    std::vector<BasicBlock *> vec;
    auto block = F.getBasicBlockList().begin();
    permutate_blocks(vec, &*block);
    if (vec.size() != 0) {
      for (auto b : vec) {
        errs() << "----------------------\n";
        errs() << *b << "\n";
      }
      assert(false);
    }
#else
    for (auto block = F.getBasicBlockList().begin();
         block != F.getBasicBlockList().end(); block++) {
      errs() << *block << "\n";
      errs() << "child number : " << get_succ_num(&*block);
      for (auto inst = block->begin(); inst != block->end(); inst++) {
        // llvm-9 seems doesn't generate phi and select
        if (isa<PHINode>(inst)) {
          errs() << "we don't use phi !\n";
          assert(false);
        }
        collect_nodes(&*inst);
      }
    }
#endif
    return false;
  }

  virtual bool doFinalization(Module &M) override {
    // 注意现在，仅仅计算了所有的函数的路径可能性
#ifdef PERMU_BLOCK
    trace_all_possible_route();

    for (auto F = M.begin(); F != M.end(); F++) {
      for (auto B = F->begin(); B != F->end(); B++) {
        for (auto inst = B->begin(); inst != B->end(); inst++) {
          if (isa<CallInst>(&(*inst)) || isa<InvokeInst>(&(*inst))) {
            CallInst *t = cast<CallInst>(&(*inst));
            if (auto f = t->getCalledFunction()) {
              if (f->getName() == "llvm.dbg.declare") {
                continue;
              }
            }
            auto fun = final_result[t];
            auto d = t->getDebugLoc();
            if (!d.isImplicitCode()) {
              errs() << d.getLine() << " ";
            } else {
              errs() << "-g"
                     << " ";
            }
            for (auto f : fun) {
              if (f != nullptr) {
                errs() << f->getName() << " ";
              } else {
                errs() << "NULL"
                       << " ";
              }
            }
            errs() << "\n";
          }
        }
      }
    }
#else
    expand_the_graph();
    for (auto F = M.begin(); F != M.end(); F++) {
      for (auto B = F->begin(); B != F->end(); B++) {
        for (auto inst = B->begin(); inst != B->end(); inst++) {
          if (isa<CallInst>(&(*inst)) || isa<InvokeInst>(&(*inst))) {
            CallInst *t = cast<CallInst>(&(*inst));
            if (auto f = t->getCalledFunction()) {
              if (f->getName() == "llvm.dbg.declare") {
                continue;
              }
            }

            Origin *ori = result[t];
            auto d = t->getDebugLoc();
            if (!d.isImplicitCode()) {
              errs() << d.getLine() << " ";
            } else {
              errs() << "-g"
                     << " ";
            }
            for (auto f : ori->fun) {
              errs() << f->getName() << " ";
            }
            if (ori->hasConstantPointerNull) {
              errs() << "NULL";
            }
            errs() << "\n";
          }
        }
      }
    }
#endif
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
