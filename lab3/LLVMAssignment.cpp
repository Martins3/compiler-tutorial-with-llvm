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

int functionPtrLevel(Type *t) {
  int level = 0;
  while (t->isPointerTy()) {
    t = t->getPointerElementType();
    level++;
  }
  if (t->isFunctionTy()) {
    return level;
  }
  return -1;
}

bool isFunctionPtrType(Type *t) { return functionPtrLevel(t) != -1; }

class FuncOrPtr {
public:
  Function *const func;
  Value *const ptr;

  FuncOrPtr(Function *f) : func(f), ptr(nullptr) {}
  FuncOrPtr(Value *v) : func(nullptr), ptr(v) {
    if (!isFunctionPtrType(v->getType())) {
      errs() << "point something unexpected !\n";
      assert(false);
    }
  }
};

struct PointToInfo {
  // TODO bottom and top, need special handler ?

  std::map<Value *, std::set<FuncOrPtr *>> point;

  PointToInfo() : point() {}
  PointToInfo(const PointToInfo &info) : point(info.point) {}

  bool operator==(const PointToInfo &info) const { return point == info.point; }

private:
  // TODO maybe delete this one ?
  void isValid(Value *v) {
    if (isa<Instruction>(v) || isa<Argument>(v)) {
      return;
    }
    errs() << "Not instruction or argument : " << *v << "\n";
    assert(false);
  }
};

std::set<CallInst *> interprocedure_call; // not all inst is interprocedure_call
std::map<Function *, std::set<BasicBlock *>> func_ret_bb;
std::map<Function *, std::set<BasicBlock *>> possible_call_site;
std::map<Value *,  GetElementPtrInst *> field_access;

// TODO init : field_access
void init_field_access(){

}

// rebuild empty log : we have a better way to handle the empty set !
//
// someone is uninit

bool updateGEP(GetElementPtrInst * gep, const PointToInfo * dfval){
  // TODO merge the point to set, if any one is not find or empty, return false.
  auto parent = gep->getPointerOperand();
  return false;
}


// TODO every function should be register already !
DataflowResult<PointToInfo>::Type pointToResult;
// TODO notice, what kept here aer not pointers

class PointToVisitor {
public:
  PointToVisitor() {}
  // meet 操作，实现为 union 的 ?
  void merge(PointToInfo *dest, const PointToInfo &src) {
    for (auto a : src.point) {
      auto v = a.first;
      auto &pts = a.second;
      auto m = dest->point;
      auto f = m.find(v);
      if (f != m.end()) {
        m[v].insert(pts.begin(), pts.end());
      } else {
        m[v] = pts;
      }
    }
  }

  /**
    void ggg(PTR* * m){
      **m = minus;
    }

    ; Function Attrs: noinline nounwind sspstrong uwtable
    define dso_local void @ggg(i32 (i32, i32)***) #0 !dbg !43 {
      call void @llvm.dbg.value(metadata i32 (i32, i32)*** %0, metadata !47,
    metadata !DIExpression()), !dbg !48 %2 = load i32 (i32, i32)**, i32 (i32,
    i32)*** %0, align 8, !dbg !49 store i32 (i32, i32)* @minus, i32 (i32, i32)**
    %2, align 8, !dbg !50 ret void, !dbg !51
    }
  */

  // TODO what a stupid function
  std::set<FuncOrPtr *> &clearOrInitPointSet(PointToInfo *dfval, Value *v) {
    auto &point = dfval->point;
    auto find = point.find(v);
    if (find != point.end()) {
      // TODO three kinds of init
      // 1. for cirital msg : easy
      // 2. for every block : must
      // 3. for every variable : init and query(query not exit, then treat it
      // as empty)
      find->second = std::set<FuncOrPtr *>(); // init empty
    } else {
      point[v] = std::set<FuncOrPtr *>(); // clear
    }
    return point[v];
  }

  bool loadStoreCheck(Value *pointer, Value *value) {
    auto p_type = pointer->getType();
    auto v_type = value->getType();
    if (isFunctionPtrType(p_type)) {
      int l_p = functionPtrLevel(p_type);
      int l_v = functionPtrLevel(v_type);
      assert(l_p = l_v + 1);
      // TODO we should check both are variable, but how ?
      return true;
    } else {
      return false;
    }
  }

  // SSA 意味着什么，如果出现了多个数值，
  // variable x point to set never decrease, not it can be !
  void compDFVal(Instruction *inst, PointToInfo *dfval) {

    // load 和 store 都是 deref
    if (auto store = dyn_cast<StoreInst>(inst)) {
      auto pointer = store->getPointerOperand();
      if(auto gep = dyn_cast<GetElementPtrInst>(pointer)){
        // TODO merge it 
      }
      
      auto value = store->getValueOperand();

      if (!loadStoreCheck(pointer, value))
        return;

      // what pointer point to : equals with p_value

      auto &point = dfval->point;
      auto find = point.find(pointer);

      if (find == point.end()) {
        // TODO merge the not find and empty logic

        // similar with callInst, caused by parameter, and we clear all the msg
        dfval = new PointToInfo();
        return;
      }
      std::set<FuncOrPtr *> &set = find->second;
      if (set.empty()) {
        // TODO can't find | find a empty one ?
        dfval = new PointToInfo();
        return;
      }

      // there are two way to dfval.point.find : for value is clearly is a
      // function make

      /*  store i32 (i32, i32)* %14, i32 (i32, i32)** %4, align 8, !dbg !98 */
      /*  %15 = load i32 (i32, i32)*, i32 (i32, i32)** %4, align 8, !dbg !99 */

      // TODO maybe merge these two situation
      if (set.size() == 1) {
        FuncOrPtr *o = *(set.begin());
        assert(o->func == nullptr);
        Value *val = o->ptr;
        assert(val != nullptr);
        std::set<FuncOrPtr *> &val_set = clearOrInitPointSet(dfval, val);
        auto find = point.find(value);
        if (find != point.end() && !find->second.empty()) {
          val_set.insert(find->second.begin(), find->second.end());
        } else {
          // caused by *x = para; TODO maybe this is correct way !
          // why we can't distinguish empty and not find ?
          dfval = new PointToInfo();
          return;
        }
      } else {
        for (auto i = set.begin(); i != set.end(); i++) {
          FuncOrPtr *o = *i;
          assert(o->func == nullptr);
          Value *val = o->ptr;
          assert(val != nullptr);
          /* std::set<FuncOrPtr *> &set = clearOrInitPointSet(dfval, val); */

          auto find = point.find(value);
          if (find != point.end() && !find->second.empty()) {
            // build new dfval
            PointToInfo temp;
            temp.point[val] = find->second;
            merge(dfval, temp);
          } else {
            // caused by *x = para; TODO maybe this is correct way !
            dfval = new PointToInfo();
            return;
          }
        }
      }
    }

    if (auto value = dyn_cast<LoadInst>(inst)) {
      auto pointer = value->getPointerOperand();
      if (!loadStoreCheck(pointer, value))
        return;
      std::set<FuncOrPtr *> &val_set = clearOrInitPointSet(dfval, value);

      auto &point = dfval->point;
      auto find = point.find(value);
      if (find != point.end() && !find->second.empty()) {
        auto &possible_values = find->second;

        for (auto possible_v : possible_values) {
          assert(possible_v->ptr != nullptr && possible_v->func == nullptr);
          auto possible_find = point.find(possible_v->ptr);
          if (possible_find != point.end() && !possible_find->second.empty()) {
            val_set.insert(possible_find->second.begin(),
                           possible_find->second.end());
          } else {
            // TODO
          }
        }

      } else {
        // TODO similar problem, this is paradiam, and we can abstruct as
        // function
      }
    }

    if (auto phi = dyn_cast<PHINode>(inst)) {
      int level = functionPtrLevel(phi->getType());
      std::set<FuncOrPtr *> &set = clearOrInitPointSet(dfval, phi);
      auto &point = dfval->point;

      /* errs() << *phi << "\n"; */
      if (level != -1) {
        /* errs() << "Parent level : " << level << "\n"; */
        for (auto v = phi->op_begin(); v != phi->op_end(); v++) {
          Value *val = v->get();
          /* errs() << "pts :" << functionPtrLevel(val->getType()) << "\n"; */
          assert(level == functionPtrLevel(val->getType()));
          if (level == 1) {
            if (auto f = dyn_cast<Function>(val)) {
              set.insert(new FuncOrPtr(f));
            }
          } else {
            auto pts = point.find(val);
            if (pts != point.end()) {
              set.insert(pts->second.begin(), pts->second.end());
            }
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
          auto df_point = dfval->point;
          auto func = df_point.find(call->getCalledValue());
          if (func != df_point.end()) {
            // FIXME maybe empty ?
            for (FuncOrPtr *v : func->second) {
              assert(v->func != nullptr);
              point_to_set.insert(v->func);
            }
          } else {
            dfval = new PointToInfo();
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
        bool hasFnPointerRet = isFunctionPtrType(call->getType());
        auto newDfVal = new PointToInfo();

        std::vector<BasicBlock *> all_call_site_bb;
        for (Function *call_site : point_to_set) {
          /* func_ret_bb.find(f); */
          auto &retBB = func_ret_bb[call_site];
          for (BasicBlock *ret : retBB) {

            // purge all the local parameter of that function ?
            // no, you can't. if the parameter is int *******a;
            merge(newDfVal, pointToResult[ret].second);

            if (hasFnPointerRet)
              all_call_site_bb.push_back(ret);
          }
        }

        // update return value
        std::set<FuncOrPtr *> &set = clearOrInitPointSet(newDfVal, call);
        for (auto ret : all_call_site_bb) {
          ReturnInst *retInst = dyn_cast<ReturnInst>(&*(ret->rbegin()));
          auto ret_sec_point = pointToResult[ret].second.point;
          auto find = ret_sec_point.find(retInst);
          if (find == ret_sec_point.end() || find->second.empty()) {
            set.insert(find->second.begin(), find->second.end());
          } else {
            set.clear();
            break;
          }
        }
        dfval = newDfVal;
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
            return true;
          }

        } else if (auto phi = dyn_cast<PHINode>(I)) {

        } else if (auto store = dyn_cast<StoreInst>(I)) {

        } else if (auto gep = dyn_cast<GetElementPtrInst>(I)) {
          /* gep->getPointerOperand(); */
          /* assert(gep->getNumIndices() == 2); */
          errs() << "The gep : " << *gep << "\n";
          errs() << gep->getNumIndices() << "\n"; // assert this value 
          errs() << *(gep->getType()) << "\n";
          continue;
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

  // TODO 4: clear this two function
  // we don't need the pred
  // we need the context
  std::vector<BasicBlock *> getBasicBlockPred(BasicBlock *b) {
    if (isFirstBB(b)) {
      return getFirstBasicBlockPred(b);
    }

    std::vector<BasicBlock *> v;
    for (pred_iterator pi = pred_begin(b), pe = pred_end(b); pi != pe; pi++) {
      v.push_back(*pi);
    }
    return v;
  }

  // get the context of current function
  std::vector<BasicBlock *> getFirstBasicBlockPred(BasicBlock *b) {
    assert(isFirstBB(b));
    std::vector<BasicBlock *> v;

    Function *F = b->getParent();
    std::vector<int> critical;
    for (auto arg = F->arg_begin(); arg != F->arg_end(); arg++) {
      if (isFunctionPtrType(arg->getType()))
        critical.push_back(arg->getArgNo());
    }

    if (critical.empty()) {
      return v;
    }

    auto site = possible_call_site.find(F);
    if (site == possible_call_site.end()) {
      possible_call_site[F] = std::set<BasicBlock *>();
    } else {
      for (auto b : site->second) {
        // TODO 注意，并没有处理递归，但是并不难处理
        auto site_pred = getBasicBlockPred(b);
        v.insert(v.end(), site_pred.begin(), site_pred.end());
      }
    }
    return v;
  }

  std::vector<int> criticalParameter(Function *F) {
    std::vector<BasicBlock *> v;
    std::vector<int> critical;
    for (auto arg = F->arg_begin(); arg != F->arg_end(); arg++) {
      if (isFunctionPtrType(arg->getType()))
        critical.push_back(arg->getArgNo());
    }
    return critical;
  }

  // treat nothing has happened, if it can't be possble, then the framework
  // breaks down ?
  //
  // how to change the name of ?
  //
  // part of parameter is usable ?
  std::vector<BasicBlock *> getFunctionCallSite(Function *F) {
    std::vector<BasicBlock *> v;
    auto site = possible_call_site.find(F);
    if (site == possible_call_site.end()) {
      possible_call_site[F] = std::set<BasicBlock *>();
    } else {
      for (auto b : site->second) {
        // TODO 注意，并没有处理递归，但是并不难处理
        /* auto site_pred = getBasicBlockPred(b); */
        /* v.insert(v.end(), site_pred.begin(), site_pred.end()); */
      }
    }
    return v;
  }

  // should I changed my mind ?
  bool isFunctionRready(Function *F) {
    // has function pointer type parameter
    // but don't have anything context (first bb pred)

    return false;
  }

  bool runOnModule(Module &M) override {

    for (auto F = M.begin(); F != M.end(); F++) {
      for (auto arg = F->arg_begin(); arg != F->arg_end(); arg++) {
      }
    }

    print_module(M);
    break_module(M);
    /* print_module(M); */

    if (true) {
      return true;
    }

    PointToVisitor visitor;
    DataflowResult<PointToInfo>::Type result;
    PointToInfo initval; // 似乎其实没用 ?

    std::set<BasicBlock *> worklist;

    // Initialize the worklist with all exit blocks

    for (auto F = M.begin(); F != M.end(); F++) {
      for (Function::iterator bi = F->begin(); bi != F->end(); ++bi) {
        BasicBlock *bb = &*bi;
        result.insert(std::make_pair(bb, std::make_pair(initval, initval)));
        worklist.insert(bb);
      }
    }

    bool changed = false;
    while (changed) {
      changed = false;
      for (auto bb : worklist) {
        // Merge all incoming value

        // TODO 我猜测在merge 的时候所有的 PointToInfo 的 key 都是不相交的。
        // 为什么不放在指令内部计算 ?
        // 对于callInst指令处理的方法 : 简单
        // 消除了 block AB 的考虑，只有 FUNC_FIRST 需要考虑，在计算pred 的时候
        //
        // 所有的block 报告自己是否发生变化，直到没有改变发生。
        //
        // 做一个 function, call instruction 的map :
        // FUNC_FIRST 处理方法是什么，找到对于block B 询问这些函数的 pred
        // 无穷递归函数 : 自己检查一下自己，参数消减 (TODO 暂时问题不大

        PointToInfo bbinval;

        /* auto pred = getBasicBlockPred(bb); */
        /* for (auto p : pred) { */
        /*   visitor.merge(&bbinval, result[p].second); */
        /* } */

        if (isFirstBB(bb)) {
          // get all the context
          // for every context, use it df value
          // handle the parameter assignment !

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
              visitor.merge(&bbinval, result[call_site->getParent()].second);
            }

            auto para = criticalParameter(curr_func);
            if (!para.empty()) {
              // purge para
              auto &point = bbinval.point;
              for (auto arg = curr_func->arg_begin();
                   arg != curr_func->arg_end(); arg++) {
                if (isFunctionPtrType(arg->getType())) {
                  point[arg] = std::set<FuncOrPtr *>();
                  // TODO why not &*arg , I'm not so sure !
                }
              }

              // rebuild parameter
              for (auto arg = curr_func->arg_begin();
                   arg != curr_func->arg_end(); arg++) {
                if (isFunctionPtrType(arg->getType())) {

                  std::set<FuncOrPtr *> &dest = point[arg];
                  for (auto call_site : all_call_site) {
                    auto call_site_bb = call_site->getParent();
                    const PointToInfo &call_site_info =
                        result[call_site_bb].first;

                    auto index = arg->getArgNo();
                    auto actual_para = call_site->User::getOperand(index);

                    auto cs_point = call_site_info.point;
                    auto find = cs_point.find(actual_para);
                    if (find != cs_point.end()) {
                      // merge
                      dest.insert(find->second.begin(), find->second.end());
                    } else {
                      dest.clear();
                      break;
                    }
                  }
                }
              }
            }

          } else {
            // nobody calls me, so bbinval is empty !
          }

        } else {
          std::vector<BasicBlock *> v;
          for (pred_iterator pi = pred_begin(bb), pe = pred_end(bb); pi != pe;
               pi++) {
            visitor.merge(&bbinval, result[bb].second);
          }
        }

        result[bb].first = bbinval;      // TODO 测试是否为深拷贝
        visitor.compDFVal(bb, &bbinval); // 将inval 装换为 exitval

        // If outgoing value changed, propagate it along the CFG
        if (bbinval == result[bb].second)
          continue;
        changed = true;
        result[bb].second = bbinval;

        // TODO 同样小心处理 (还是先不处理吧 ? 创建一个 interface 出来即可
        for (auto pi = succ_begin(bb), pe = succ_end(bb); pi != pe; pi++) {
          worklist.insert(*pi);
        }
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
