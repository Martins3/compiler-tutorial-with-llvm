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

#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/raw_ostream.h"
#include <llvm/IR/Function.h>
#include <llvm/Pass.h>

#include "Dataflow.h"
using namespace llvm;

struct LivenessInfo {
  std::set<Instruction *> LiveVars; /// Set of variables which are live
  LivenessInfo() : LiveVars() {}
  LivenessInfo(const LivenessInfo &info) : LiveVars(info.LiveVars) {}

  bool operator==(const LivenessInfo &info) const {
    return LiveVars == info.LiveVars;
  }
};

// 对于指令循环打印而已
inline raw_ostream &operator<<(raw_ostream &out, const LivenessInfo &info) {
  for (std::set<Instruction *>::iterator ii = info.LiveVars.begin(),
                                         ie = info.LiveVars.end();
       ii != ie; ++ii) {
    const Instruction *inst = *ii;
    out << inst->getName();
    out << " ";
    // https://llvm.org/doxygen/classllvm_1_1Value.html#adb5c319f5905c1d3ca9eb5df546388c5
    errs() << "---" << *inst << "---\n";
    out << "wdnmd";
  }
  return out;
}

// TODO  DataflowVisitor
// 1. 提供的接口是什么 ?
// 2. 能够解决的问题的范围是什么 ?
class LivenessVisitor : public DataflowVisitor<struct LivenessInfo> {
public:
  LivenessVisitor() {}
  // meet 操作，实现为 union 的 ?
  void merge(LivenessInfo *dest, const LivenessInfo &src) override {
    for (std::set<Instruction *>::const_iterator ii = src.LiveVars.begin(),
                                                 ie = src.LiveVars.end();
         ii != ie; ++ii) {
      dest->LiveVars.insert(*ii);
    }
  }

  // 装换函数，但是是针对于 instruction 
  // block 的 compDFVal 只是简单的针对于instruction 的堆积
  void compDFVal(Instruction *inst, LivenessInfo *dfval) override {
    if (isa<DbgInfoIntrinsic>(inst))
      return;
    dfval->LiveVars.erase(inst);
    for (User::op_iterator oi = inst->op_begin(), oe = inst->op_end(); oi != oe;
         ++oi) {
      Value *val = *oi;
      if (isa<Instruction>(val))
        dfval->LiveVars.insert(cast<Instruction>(val));
    }
  }
};

class Liveness : public FunctionPass {
public:
  static char ID;
  Liveness() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override {
    /* F.dump(); */
    errs() << F << "\n";
    LivenessVisitor visitor;
    DataflowResult<LivenessInfo>::Type result;
    LivenessInfo initval;

    compBackwardDataflow(&F, &visitor, &result, initval);
    printDataflowResult<LivenessInfo>(errs(), result);
    return false;
  }
};
