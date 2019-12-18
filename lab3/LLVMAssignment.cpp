//===- Hello.cpp - Example code from "Writing an LLVM Pass"
//---------------===//
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
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/ToolOutputFile.h>

#if LLVM_VERSION_MAJOR >= 4
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/Bitcode/BitcodeWriter.h>

#else
#include <llvm/Bitcode/ReaderWriter.h>
#endif

#include <llvm/Transforms/Scalar.h>

#include "Dataflow.h"
#include "Liveness.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils.h"
#include <iostream>
#include <llvm/IR/Function.h>
#include <llvm/Pass.h>
using namespace llvm;
#if LLVM_VERSION_MAJOR >= 4
static ManagedStatic<LLVMContext> GlobalContext;
static LLVMContext &getGlobalContext() { return *GlobalContext; }
#endif

/* In LLVM 5.0, when  -O0 passed to clang , the functions generated with clang
 * will have optnone attribute which would lead to some transform passes
 * disabled, like mem2reg.
 */
#if LLVM_VERSION_MAJOR == 5
struct EnableFunctionOptPass : public FunctionPass {
  static char ID;
  EnableFunctionOptPass() : FunctionPass(ID) {}
  bool runOnFunction(Function &F) override {
    if (F.hasFnAttribute(Attribute::OptimizeNone)) {
      F.removeFnAttr(Attribute::OptimizeNone);
    }
    return true;
  }
};

char EnableFunctionOptPass::ID = 0;
#endif

class PointToInfo {
private:
  std::map<Value *, std::set<Value *>> point;

public:
  PointToInfo() : point() {}
  PointToInfo(const PointToInfo &info) : point(info.point) {}

  bool operator==(const PointToInfo &info) const { return point == info.point; }

  std::set<Value *> &getPTS(Value *v) {
    assert(isNotEmpty(v));
    return point.find(v)->second;
  }

  void setNotReady(Value *v) { point.erase(v); }

  void shutdown() { point.clear(); }

  bool isNotEmpty(Value *v) {
    auto f = point.find(v);
    return f != point.end() && !f->second.empty();
  }

  std::set<Value *> &clear(Value *v) {
    point[v] = std::set<Value *>();
    return point[v];
  }

  void merge(const PointToInfo &src) {
    for (auto a : src.point) {
      auto v = a.first;
      auto &pts = a.second;
      auto f = point.find(v);
      if (f != point.end()) {
        point[v].insert(pts.begin(), pts.end());
      } else {
        point[v] = pts;
      }
    }
  }

  void setupConstantPTS(Value *v) {
    if (auto f = dyn_cast<Constant>(v)) {
      clear(v).insert(v);
    }
  }
};

std::set<CallInst *> interprocedure_call;
std::map<Function *, std::set<BasicBlock *>> func_ret_bb;
std::map<Function *, std::set<BasicBlock *>> possible_call_site;
DataflowResult<PointToInfo>::Type pointToResult;

class PointToVisitor {
public:
  PointToVisitor() {}
  // meet 操作，实现为 union 的 ?

  void merge(PointToInfo *dest, const PointToInfo &src) { dest->merge(src); }

  void compDFVal(Instruction *inst, PointToInfo *dfval) {
    if (auto gep = dyn_cast<GetElementPtrInst>(inst)) {
      dfval->clear(gep).insert(gep->getPointerOperand());
    }

    // *x = y
    if (auto store = dyn_cast<StoreInst>(inst)) {
      auto pointer = store->getPointerOperand();
      auto value = store->getValueOperand();

      dfval->setupConstantPTS(value);

      if (!dfval->isNotEmpty(pointer)) {
        dfval->shutdown(); // store to anywhere, crash it!
        return;
      }

      std::set<Value *> &set = dfval->getPTS(pointer);

      PointToInfo temp;
      for (auto i = set.begin(); i != set.end(); i++) {
        if (set.size() == 1) {
          Value *o = *(set.begin());
          dfval->clear(o);
        }

        if (!dfval->isNotEmpty(value)) {
          // many one rely on me, but I'am not ready.
          dfval->shutdown();
          return;
        } else {
          std::set<Value *> &my_set = dfval->getPTS(value);
          temp.getPTS(*i) = my_set;
        }
      }
      merge(dfval, temp);
    }

    // y = *x
    if (auto location = dyn_cast<LoadInst>(inst)) {
      auto pointer = location->getPointerOperand();

      std::set<Value *> &val_set = dfval->clear(location);

      if (!dfval->isNotEmpty(pointer)) {
        dfval->setNotReady(location);
        return;
      } else {
        std::set<Value *> &possible_values = dfval->getPTS(pointer);
        for (auto possible_v : possible_values) {
          if (!dfval->isNotEmpty(possible_v)) {
            dfval->setNotReady(location);
            return;
          } else {
            std::set<Value *> &possible_find = dfval->getPTS(possible_v);
            val_set.insert(possible_find.begin(), possible_find.end());
          }
        }
      }

      if (auto phi = dyn_cast<PHINode>(inst)) {
        /* errs() << *phi << "\n"; */
        std::set<Value *> &set = dfval->clear(phi);
        /* errs() << "Parent level : " << level << "\n"; */

        for (auto v = phi->op_begin(); v != phi->op_end(); v++) {
          Value *val = v->get();
          dfval->setupConstantPTS(val);
          /* errs() << "pts :" << functionPtrLevel(val->getType()) << "\n"; */

          // empty caused by : not ready or current is pointer !
          if (dfval->isNotEmpty(val)) {
            std::set<Value *> &pts = dfval->getPTS(val);
            set.insert(pts.begin(), pts.end());
          } else {
            dfval->setNotReady(phi);
            return;
          }
        }
      }
    }

    if (auto call = dyn_cast<CallInst>(inst)) {
      if (interprocedure_call.find(call) != interprocedure_call.end()) {
        auto bb = call->getParent();
        assert(&*bb->begin() == call);

        // init point_to_set
        std::set<Function *> point_to_set;
        if (auto f = call->getFunction()) {
          point_to_set.insert(f);
        } else {
          auto caller = call->getCalledValue();
          if (dfval->isNotEmpty(caller)) {
            std::set<Value *> func = dfval->getPTS(caller);
            for (Value *v : func) {
              if (auto f = dyn_cast<Function>(v)) {
                point_to_set.insert(f);
              } else {
                errs() << "This is a call inst, it should be a function\n";
                assert(false);
              }
            }
          } else {
            dfval->shutdown();
            return;
          }
        }

        // update possible_call_site
        auto call_bb = call->getParent();
        for (auto call_site : possible_call_site) {
          auto a = call_site.first;
          auto &b = call_site.second;
          if (point_to_set.find(a) != point_to_set.end()) {
            b.insert(call_bb);
          } else {
            b.erase(call_bb);
          }
        }

        // handle return value
        bool hasFnPointerRet = call->getType()->isVoidTy();
        dfval->shutdown();

        std::vector<BasicBlock *> all_call_site_bb;
        for (Function *call_site : point_to_set) {
          /* func_ret_bb.find(f); */
          auto &retBB = func_ret_bb[call_site];
          for (BasicBlock *ret : retBB) {
            // purge all the local parameter of that function ?
            // no, you can't. if the parameter is int *******a;
            merge(dfval, pointToResult[ret].second);
            if (hasFnPointerRet)
              all_call_site_bb.push_back(ret);
          }
        }

        // update return value
        std::set<Value *> &set = dfval->clear(call);
        for (auto ret : all_call_site_bb) {
          ReturnInst *retInst = dyn_cast<ReturnInst>(&*(ret->rbegin()));
          auto retVal = retInst->getReturnValue();
          pointToResult[ret].second.setupConstantPTS(retVal);

          if (pointToResult[ret].second.isNotEmpty(retInst)) {
            std::set<Value *> t = pointToResult[ret].second.getPTS(retInst);
            set.insert(t.begin(), t.end());
          } else {
            dfval->setNotReady(call);
            return;
          }
        }
      }
    }
  }

  void compDFVal(BasicBlock *block, PointToInfo *dfval) {
    for (BasicBlock::iterator ii = block->begin(), ie = block->end(); ii != ie;
         ++ii) {
      Instruction *inst = &*ii;
      compDFVal(inst, dfval);
    }
  }
};

///!TODO TO BE COMPLETED BY YOU FOR ASSIGNMENT 3
struct FuncPtrPass : public ModulePass {
  static char ID; // Pass identification, replacement for typeid
  FuncPtrPass() : ModulePass(ID) {}

  std::set<StringRef> func;

  bool break_call(CallInst *call) {
    if (auto f = call->getCalledFunction()) {
      if (func.find(f->getName()) == func.end()) {
        return false;
      }
    }
    return true;
  }

  void debug_helper_call(CallInst *call) {

    if (call->getFunction()->isIntrinsic())
      return;

    errs() << *call << "<----\n";
    errs() << "getCalledValue" << *(call->getCalledValue()) << "\n";
    errs() << "getName" << call->getName() << "\n";

    /* int n = call->getNumOperands(); */
    /* n = call->getNumArgOperands(); */
    /* for (int i = 0; i < n; ++i) { */
    /*   Value *v = call->getOperand(i); */
    /*   errs() << *v << "\n"; */
    /* } */
    /* errs() << "-------]\n"; */
  }

  bool break_function(Function *F) {
    for (auto B = F->begin(); B != F->end(); B++) {
      for (auto I = B->begin(); I != B->end(); I++) {
        if (auto call = dyn_cast<CallInst>(I)) {
          /* debug_helper_call(call); */
          continue;
          if (break_call(&*call)) {
            /* pending.push_back(std::make_pair(&*B, call)); */
            if (&*(B->begin()) == &(*I)) {
              continue;
            }
            B->splitBasicBlock(I);
            interprocedure_call.insert(call);
            return true;
          }

        } else if (auto phi = dyn_cast<PHINode>(I)) {

        } else if (auto store = dyn_cast<StoreInst>(I)) {
          auto v = store->getValueOperand();
          if (dyn_cast<Constant>(v)) {
            errs() << "The store : " << *store << "\n";
            errs() << *v << "\n";
          }

        } else if (auto gep = dyn_cast<GetElementPtrInst>(I)) {

          /* gep->getPointerOperand(); */
          /* assert(gep->getNumIndices() == 2); */
          continue;
          errs() << "The gep : " << *gep << "\n";
          errs() << gep->getNumIndices() << "\n"; // assert this value
          errs() << *(gep->getType()) << "\n";
          for (auto idx = gep->idx_begin(); idx != gep->idx_end(); idx++) {
            errs() << "idx : " << *(idx->get()) << "\n";
          }

          for (auto op = gep->op_begin(); op != gep->op_end(); op++) {
            errs() << "op : " << *(op->get());
            errs() << " : " << *(op->get()->getType()) << "\n";
          }
        }
      }
    }
    return false;
  }

  void break_module(Module &M) {
    for (auto F = M.begin(); F != M.end(); F++) {
      func.insert(F->getName());
      for (auto B = F->begin(); B != F->end(); B++) {
        auto si = succ_begin(&*B), se = succ_end(&*B);
        if (si == se) {
          func_ret_bb[&*F].insert(&*B);
        }
      }
    }
    for (auto F = M.begin(); F != M.end(); F++) {
      while (break_function(&*F))
        ;
    }
  }

  void print_module(Module &M) {
    errs() << "----------------- print module --------------\n";
    for (auto F = M.begin(); F != M.end(); F++) {
      errs() << "--";
      errs() << *F << "\n";
      errs() << "--";
    }
  }

  bool isFirstBB(BasicBlock *b) { return &*(b->getParent()->begin()) == b; }

  bool runOnModule(Module &M) override {
    for (auto F = M.begin(); F != M.end(); F++) {
      for (auto arg = F->arg_begin(); arg != F->arg_end(); arg++) {
      }
    }

    print_module(M);
    break_module(M);
    /* print_module(M); */

    return true;

    PointToVisitor visitor;
    PointToInfo initval; // 似乎其实没用 ?

    std::set<BasicBlock *> worklist;

    // Initialize the worklist with all exit blocks

    for (auto F = M.rbegin(); F != M.rend(); F++) {
      for (Function::iterator bi = F->begin(); bi != F->end(); ++bi) {
        BasicBlock *bb = &*bi;
        pointToResult.insert(
            std::make_pair(bb, std::make_pair(initval, initval)));
        worklist.insert(bb);
      }
    }

    bool changed = false;
    while (changed) {
      changed = false;
      for (auto bb : worklist) {
        // Merge all incoming value

        PointToInfo bbinval;

        if (isFirstBB(bb)) {
          Function *curr_func = bb->getParent();

          auto find = possible_call_site.find(curr_func);
          if (find != possible_call_site.end()) {

            auto &call_site = find->second;
            std::vector<CallInst *> all_call_site;
            for (BasicBlock *b : call_site) {
              CallInst *call = dyn_cast<CallInst>(&*(b->begin()));
              if (call != nullptr) {
                if (interprocedure_call.find(call) !=
                    interprocedure_call.end()) {
                  all_call_site.push_back(call);
                } else {
                  assert(false);
                }
              } else {
                assert(false);
              }
            }

            // merge
            for (auto call_site : all_call_site) {
              visitor.merge(&bbinval,
                            pointToResult[call_site->getParent()].second);
            }

            for (auto arg = curr_func->arg_begin(); arg != curr_func->arg_end();
                 arg++) {
              std::set<Value *> &dest = bbinval.clear(arg);
              for (auto call_site : all_call_site) {
                auto call_site_bb = call_site->getParent();
                PointToInfo &call_site_info = pointToResult[call_site_bb].first;
                auto actual_para = call_site->User::getOperand(arg->getArgNo());
                call_site_info.setupConstantPTS(actual_para);
                if (call_site_info.isNotEmpty(actual_para)) {
                  auto &t = call_site_info.getPTS(actual_para);
                  dest.insert(t.begin(), t.end());
                } else {
                  bbinval.setNotReady(arg);
                  break;
                }
              }
            }
          } else {
            // nobody calls me, so bbinval is empty !
            // nothing should be down !
          }

        } else {
          std::vector<BasicBlock *> v;
          for (pred_iterator pi = pred_begin(bb), pe = pred_end(bb); pi != pe;
               pi++) {
            visitor.merge(&bbinval, pointToResult[bb].second);
          }
        }

        pointToResult[bb].first = bbinval; // TODO 测试是否为深拷贝
        visitor.compDFVal(bb, &bbinval);

        // If outgoing value changed, propagate it along the CFG
        if (bbinval == pointToResult[bb].second)
          continue;
        changed = true;
        pointToResult[bb].second = bbinval;
      }
    }

    for (auto F = M.begin(); F != M.end(); F++) {
      for (auto B = F->begin(); B != F->end(); B++) {
        for (auto I = B->begin(); I != B->end(); I++) {
          errs() << "please check every find before coding to here\n";
          // 输出结果，利用initval 的结果
          // 部分指令直接打印
        }
      }
    }

    return false;
  }
};

char FuncPtrPass::ID = 0;
static RegisterPass<FuncPtrPass> X("funcptrpass",
                                   "Print function call instruction");

char Liveness::ID = 0;
static RegisterPass<Liveness> Y("liveness", "Liveness Dataflow Analysis");

static cl::opt<std::string>
    InputFilename(cl::Positional, cl::desc("<filename>.bc"), cl::init(""));

int main(int argc, char **argv) {
  LLVMContext &Context = getGlobalContext();
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
#if LLVM_VERSION_MAJOR == 5
  Passes.add(new EnableFunctionOptPass());
#endif
  /// Transform it to SSA
  Passes.add(llvm::createPromoteMemoryToRegisterPass());

  /// Your pass to print Function and Call Instructions
  /* Passes.add(new Liveness()); */
  Passes.add(new FuncPtrPass());
  Passes.run(*M.get());
#ifndef NDEBUG
  exit(0);
  system("pause");
#endif
}
