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

bool changed = true;
class PointToInfo {
private:
  std::map<Value *, std::set<Value *>> point;
  std::vector<Value *> pending;

public:
  PointToInfo() : point() {}
  PointToInfo(const PointToInfo &info) : point(info.point) {}

  bool operator==(const PointToInfo &info) const { return point == info.point; }

  std::set<Value *> &getPTS(Value *v) {
    assert(isNotEmpty(v));
    return point[v];
  }

  void pend(Value *v) { pending.push_back(v); }

  void setup(Value *declare) {
    auto v = pending.back();
    pending.pop_back();
    clear(v).insert(declare);
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
    for (auto &a : src.point) {
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

  void show(raw_ostream &out) const {
    for (auto m : point) {
      auto key = m.first;
      if (auto f = dyn_cast<Function>(key)) {
        out << "  ->" << f->getName() << " : {";
      } else {
        out << "->" << *(key) << " : { ";
      }
      for (auto val : m.second) {
        if (auto f = dyn_cast<Function>(val)) {
          out << "  " << f->getName();
        } else {
          out << *(val);
        }
      }
      out << "}\n";
    }
  }
};

inline raw_ostream &operator<<(raw_ostream &out, const PointToInfo &info) {
  info.show(out);
  return out;
}

inline raw_ostream &operator<<(raw_ostream &out, const std::set<Value *> &set) {
  out << "********\n";
  for (auto val : set) {
    if (auto f = dyn_cast<Function>(val)) {
      out << f->getName() << "\n";
    } else {
      out << *(val) << "\n";
    }
  }
  return out;
}

std::set<CallInst *> interprocedure_call;
std::map<Function *, std::set<BasicBlock *>> func_ret_bb;
std::map<Function *, std::set<CallInst *>> possible_call_site;
DataflowResult<PointToInfo>::Type pointToResult;

std::map<AllocaInst *, CallInst *> alloc_related;

void init_alloc_related(BasicBlock *B) {
  // the alloc instruction and llvm.dbg.declare is always in the same block
  std::vector<AllocaInst *> v;
  for (auto I = B->begin(); I != B->end(); I++) {
    if (auto call = dyn_cast<CallInst>(I)) {
      if (auto f = call->getCalledFunction()) {
        if (f->isIntrinsic()) {
          if (f->getName() == "llvm.dbg.declare") {
            auto alloc = v.back();
            v.pop_back();
            alloc_related[alloc] = call;
          }
        }
      }
    }

    else if (auto alloc = dyn_cast<AllocaInst>(I)) {
      v.push_back(alloc);
    }
  }

  assert(v.empty());
}

void debug_possible_call_site() {
  errs() << __FUNCTION__ << "\n";
  for (auto call_site : possible_call_site) {
    auto a = call_site.first;
    auto &b = call_site.second;
    if (!b.empty()) {
      errs() << a->getName();
      for (auto bb : b) {
        errs() << *bb;
      }
    }
  }
}

std::vector<Function *> get_possible_call(CallInst *call) {
  std::vector<Function *> v;
  for (auto &k : possible_call_site) {
    if (k.second.find(call) != k.second.end()) {
      v.push_back(k.first);
    }
  }
  return v;
}

class PointToVisitor {
public:
  PointToVisitor() {}
  void merge(PointToInfo *dest, const PointToInfo &src) { dest->merge(src); }

  // *x = y
  void store_handler(StoreInst *store, PointToInfo *dfval) {
    auto pointer = store->getPointerOperand();
    auto value = store->getValueOperand();
    dfval->setupConstantPTS(value);

    /* errs() << "store : " << *store << "\n"; */
    if (!dfval->isNotEmpty(pointer)) {
      // reach here can't happended if we transfer in the reverse order
      /* errs() << "POINT TO SET BEGIN\n"; */
      /* errs() << *dfval << "\n"; */
      /* errs() << "POINT TO SET END\n"; */
      /* errs() << *pointer << "\n"; */
      /* errs() << *store << "\n"; */
      /* std::cout << __LINE__ << std::endl; */
      /* changed = true; */
      dfval->shutdown(); // store to anywhere, crash it!
      return;
    }

    /* errs() << *dfval; */

    std::set<Value *> &set = dfval->getPTS(pointer);

    PointToInfo temp;
    std::vector<Value *> v;
    v.insert(v.begin(), set.begin(), set.end());
    for (auto p : v) {
      if (set.size() == 1) {
        Value *o = *(set.begin());
        dfval->clear(o);
      }

      if (!dfval->isNotEmpty(value)) {
        dfval->setNotReady(p);
      } else {
        temp.clear(p) = dfval->getPTS(value);
      }
    }
    merge(dfval, temp);
  }

  // y = *x
  void load_handler(LoadInst *location, PointToInfo *dfval) {
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
  }

  void call_handler(CallInst *call, PointToInfo *dfval) {
    if (interprocedure_call.find(call) == interprocedure_call.end())
      return;

    auto bb = call->getParent();
    assert(&*bb->begin() == call);

    // init point_to_set
    std::set<Function *> point_to_set;
    if (auto f = call->getCalledFunction()) {
      point_to_set.insert(f);
    } else {
      auto caller = call->getCalledValue();
      if (dfval->isNotEmpty(caller)) {
        std::set<Value *> func = dfval->getPTS(caller);
        for (Value *v : func) {
          if (auto f = dyn_cast<Function>(v)) {
            point_to_set.insert(f);
          } else {
            errs() << *call << "\n";
            errs() << *v << "\n";
            errs() << "This is a call inst, it should be a function\n";
            assert(false);
          }
        }
      } else {
        dfval->shutdown();
        /* std::cout << __LINE__ << std::endl; */
        changed = true;
        return;
      }
    }

    // FIXME When call graph upated, we should run it again
    //
    // update possible_call_site
    for (auto &call_site : possible_call_site) {
      auto a = call_site.first;
      auto &b = call_site.second;
      if (point_to_set.find(a) != point_to_set.end()) {
        if (b.find(call) == b.end()) {
          b.insert(call);
          changed = true;
        }
      } else {
        if (b.find(call) != b.end()) {
          b.erase(call);
          changed = true;
        }
      }
    }

    // handle return value
    bool hasFnPointerRet = !call->getType()->isVoidTy();
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
      /* errs() << "7777777777\n"; */
      /* errs() << ret->getParent()->getName() << "\n"; */
      /* errs() << "call : " << *call; */
      /* errs() << *ret << "\n"; */
      /* errs() << "BEGIN PTS\n"; */
      /* errs() << pointToResult[ret].second; */
      /* errs() << "*********\n"; */
      /* errs() << pointToResult[ret].first; */
      /* errs() << "END PTS\n"; */

      ReturnInst *retInst = dyn_cast<ReturnInst>(&*(ret->rbegin()));
      assert(retInst != nullptr);

      auto retVal = retInst->getReturnValue();
      pointToResult[ret].second.setupConstantPTS(retVal);

      if (pointToResult[ret].second.isNotEmpty(retVal)) {
        std::set<Value *> t = pointToResult[ret].second.getPTS(retVal);
        set.insert(t.begin(), t.end());
      } else {
        dfval->setNotReady(call);
        return;
      }
    }
  }

  void phi_handler(PHINode *phi, PointToInfo *dfval) {
    std::set<Value *> &set = dfval->clear(phi);

    for (auto v = phi->op_begin(); v != phi->op_end(); v++) {
      Value *val = v->get();
      if (dyn_cast<ConstantPointerNull>(val))
        continue;
      dfval->setupConstantPTS(val);

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

  void compDFVal(Instruction *inst, PointToInfo *dfval) {
    if (auto gep = dyn_cast<GetElementPtrInst>(inst)) {
      /* dfval->clear(gep).insert(gep->getPointerOperand()); */
      /* return; */

      auto p = gep->getPointerOperand();
      if (dfval->isNotEmpty(p))
        dfval->clear(gep) = dfval->getPTS(p);
      else {
        /* errs() << *p << "\n"; */
        /* errs() << *gep << "\n"; */
        /* errs() << *dfval << "\n"; */
        dfval->setNotReady(gep);
        /* errs() << *inst << "\n"; */
        /* errs() << *p << "\n"; */
      }
    }

    if (auto bitcast = dyn_cast<BitCastInst>(inst)) {
      /* errs() << "777777" << "\n"; */
      Value *from = bitcast->User::getOperand(0);

      /* errs() << *from << "\n"; */

      dfval->clear(bitcast).insert(from);
    }

    else if (auto alloc = dyn_cast<AllocaInst>(inst)) {
      auto f = alloc_related[alloc];
      assert(f != nullptr);
      dfval->clear(alloc).insert(f);
    }

    else if (auto store = dyn_cast<StoreInst>(inst)) {
      store_handler(store, dfval);
    }

    else if (auto location = dyn_cast<LoadInst>(inst)) {
      load_handler(location, dfval);
    }

    else if (auto phi = dyn_cast<PHINode>(inst)) {
      phi_handler(phi, dfval);
    }

    else if (auto call = dyn_cast<CallInst>(inst)) {
      call_handler(call, dfval);
    }
  }

  void compDFVal(BasicBlock *block, PointToInfo *dfval) {
    for (BasicBlock::iterator ii = block->begin(), ie = block->end(); ii != ie;
         ++ii) {
      Instruction *inst = &*ii;
      compDFVal(inst, dfval);
      /* errs() << *inst << "\n"; */
      /* errs() << *dfval; */
    }
  }
};

///!TODO TO BE COMPLETED BY YOU FOR ASSIGNMENT 3
struct FuncPtrPass : public ModulePass {
  static char ID; // Pass identification, replacement for typeid
  FuncPtrPass() : ModulePass(ID) {}

  bool break_call(CallInst *call) {
    if (auto f = call->getCalledFunction()) {
      if (f->isIntrinsic())
        return false;
      if (f->isDeclaration()) {
        return false;
      }
    }
    return true;
  }

  bool break_function(Function *F) {
    for (auto B = F->begin(); B != F->end(); B++) {
      for (auto I = B->begin(); I != B->end(); I++) {
        if (auto call = dyn_cast<CallInst>(I)) {
          if (break_call(&*call)) {
            if (&*(B->begin()) == &(*I)) {
              continue;
            }
            B->splitBasicBlock(I);
            return true;
          }
        }
      }
    }
    return false;
  }

  void break_module(Module &M) {
    for (auto F = M.begin(); F != M.end(); F++) {
      while (break_function(&*F))
        ;
    }

    for (auto F = M.begin(); F != M.end(); F++) {
      possible_call_site[&*F] = std::set<CallInst *>();
      for (auto B = F->begin(); B != F->end(); B++) {
        init_alloc_related(&*B);
        auto si = succ_begin(&*B), se = succ_end(&*B);
        if (si == se) {
          func_ret_bb[&*F].insert(&*B); // init
        }
      }
    }

    for (auto F = M.begin(); F != M.end(); F++) {
      for (auto B = F->begin(); B != F->end(); B++) {
        for (auto I = B->begin(); I != B->end(); I++) {
          if (auto call = dyn_cast<CallInst>(I)) {
            if (break_call(&*call)) {
              interprocedure_call.insert(call); // init
            }
          }
        }
      }
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

  void debug_interprocedure_call() {
    // something wired to handle
    errs() << __FUNCTION__ << "\n";
    for (auto k : interprocedure_call) {
      errs() << *k << "\n";
    }
  }

  bool isFirstBB(BasicBlock *b) { return &*(b->getParent()->begin()) == b; }

/* #define MY_DEBUG 1 */
  bool runOnModule(Module &M) override {
    break_module(M);

#ifdef MY_DEBUG
    print_module(M);
#endif
    PointToVisitor visitor;
    PointToInfo initval;

    std::set<BasicBlock *> worklist;

    // Initialize the worklist with all exit blocks

    for (auto F = M.rbegin(); F != M.rend(); F++) {
      for (Function::iterator bi = F->begin(); bi != F->end(); ++bi) {
        BasicBlock *bb = &*bi;
        pointToResult.insert(
            std::make_pair(bb, std::make_pair(initval, initval)));
        worklist.insert(bb);
        /* goto begin; */
      }
    }

    /* begin: */
    while (changed) {
      changed = false;
      for (auto bb : worklist) {
        // Merge all incoming value
        PointToInfo bbinval;

        if (isFirstBB(bb)) {
          Function *curr_func = bb->getParent();
          auto &call_site = possible_call_site[curr_func];
          if (!call_site.empty()) {
            std::vector<CallInst *> all_call_site;
            for (CallInst *call : call_site) {
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
                            pointToResult[call_site->getParent()].first);
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
                  /* errs() << "WOWOWOWOWO\n"; */
                  /* errs() << bbinval; */
                } else {
                  // YES! even some parameter is not ready
                  // dataflow is still valid
                  //
                  /* errs() << "WOWOWOWOWO\n"; */
                  /* errs() << *actual_para << " : actual_para \n"; */
                  /* errs() << call_site_info; */
                  /* for(auto x : all_call_site){ */
                  /*   errs() << x->getParent()->getParent()->getName() << " :
                   * call site <-\n"; */
                  /* } */
                  /* errs() << "current function : " << curr_func->getName();
                   */
                  /* errs() << *arg << "\n"; */
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
          for (pred_iterator pi = pred_begin(bb), pe = pred_end(bb); pi != pe;
               pi++) {
            visitor.merge(&bbinval, pointToResult[*pi].second);
          }
        }

        pointToResult[bb].first = bbinval;
        visitor.compDFVal(bb, &bbinval);

        // If outgoing value changed, propagate it along the CFG
        if (bbinval == pointToResult[bb].second) {
          continue;
        }
        changed = true;
        pointToResult[bb].second = bbinval;
      }
    }

#ifdef MY_DEBUG
    printDataflowResult<PointToInfo>(errs(), pointToResult);
#endif

    for (auto F = M.begin(); F != M.end(); F++) {
      for (auto B = F->begin(); B != F->end(); B++) {
        for (auto I = B->begin(); I != B->end(); I++) {
          if (auto call = dyn_cast<CallInst>(I)) {
            if (auto f = call->getCalledFunction())
              if (f->isIntrinsic())
                continue;
            std::vector<Function *> v;
            if (auto f = call->getCalledFunction()) {
              v.push_back(f);
            } else {
              v = get_possible_call(call);
            }

            auto d = call->getDebugLoc();
            if (!d.isImplicitCode()) {
              errs() << d.getLine() << " : ";
            }
            for (auto f : v) {
              errs() << f->getName() << " ";
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
