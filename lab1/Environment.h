//==--- tools/clang-check/ClangInterpreter.cpp - Clang Interpreter tool
//--------------===//
//===----------------------------------------------------------------------===//
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Decl.h"
#include "clang/AST/EvaluatedExprVisitor.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"
#include <iostream>

#define TODO()                                                                 \
  fprintf(stderr, "TODO \n");                                                  \
  assert(false);

#define INFO(msg)                                                              \
  fprintf(stderr, "info: %s:%d: ", __FILE__, __LINE__);                        \
  fprintf(stderr, "%s\n", msg);

using namespace clang;

class InterpreterVisitor;

typedef long long int lld;
class StackFrame {
  /// StackFrame maps Variable Declaration to Value
  /// Which are either integer or addresses (also represented using an Integer
  /// value)
  std::map<Decl *, lld> mVars;
  std::map<Stmt *, lld> mExprs;
  /// The current stmt
  Stmt *mPC;
  lld returnVal;

public:
  StackFrame() : mVars(), mExprs(), mPC(), returnVal(4397) {}

  lld getReturnVal() { return returnVal; }
  void setReturnVal(lld ret) { returnVal = ret; }

  // Decl 和 Stmt 有什么区别
  void bindDecl(Decl *decl, lld val) {
    // fprintf(stderr, "%s %lld\n", "bind decl ", val);
    // decl->dump();
    mVars[decl] = val;
  }
  lld getDeclVal(Decl *decl) {
    if (mVars.find(decl) == mVars.end()) {
      fprintf(stderr, "%s\n", "shit! where is the decl :");
      decl->dump();
      assert(mVars.find(decl) != mVars.end());
    }
    return mVars.find(decl)->second;
  }

  void bindStmt(Stmt *stmt, lld val) {
    // fprintf(stderr, "%s %lld\n", "bind stmt ", val);
    // stmt->dump();
    mExprs[stmt] = val;
  }

  lld getStmtVal(Stmt *stmt) {
    if (mExprs.find(stmt) == mExprs.end()) {
      fprintf(stderr, "%s\n", "shit! we can't the find the stmt :");
      stmt->dump();
      assert(mExprs.find(stmt) != mExprs.end());
    }
    return mExprs[stmt];
  }

  // TODO 并不知道其作用是什么 ?
  void setPC(Stmt *stmt) { mPC = stmt; }
  Stmt *getPC() { return mPC; }
};

/// Heap maps address to a value
class Heap {
  std::map<Stmt *, lld> mExprs;

public:
  Heap() : mExprs() {}
  void bindStmt(Stmt *stmt, lld val) {
    // fprintf(stderr, "%s %lld\n", "bind stmt ", val);
    // stmt->dump();
    mExprs[stmt] = val;
  }

  lld getStmtVal(Stmt *stmt) {
    if (mExprs.find(stmt) == mExprs.end()) {
      // fprintf(stderr, "%s\n", "Heap faile to find the address of *a :");
      // stmt->dump();
      // 目前认为只有cast 才会导致 *x 类型进行迁移
      // 但是不是所有的cast 都是 *x 类型的
      return 0;
    }
    return mExprs[stmt];
  }
};

class Environment;

class InterpreterVisitor : public EvaluatedExprVisitor<InterpreterVisitor> {
public:
  explicit InterpreterVisitor(const ASTContext &context, Environment *env)
      : EvaluatedExprVisitor(context), mEnv(env) {}
  virtual ~InterpreterVisitor() {}

  Environment *mEnv;
  virtual void VisitBinaryOperator(BinaryOperator *bop);
  virtual void VisitUnaryOperator(UnaryOperator *uo);
  virtual void VisitDeclRefExpr(DeclRefExpr *expr);
  virtual void VisitUnaryExprOrTypeTraitExpr(UnaryExprOrTypeTraitExpr *tt);
  virtual void VisitCastExpr(CastExpr *expr);
  virtual void VisitArraySubscriptExpr(ArraySubscriptExpr *array);
  virtual void VisitCallExpr(CallExpr *call);
  virtual void VisitIntegerLiteral(IntegerLiteral *i);
  virtual void VisitReturnStmt(ReturnStmt *ret);
  virtual void VisitIfStmt(IfStmt *ifstmt);
  virtual void VisitWhileStmt(WhileStmt *ws);
  virtual void VisitForStmt(ForStmt *f);
  virtual void VisitDeclStmt(DeclStmt *declstmt);
  virtual void VisitParenExpr(ParenExpr *vardecl);
};

class Environment {
  // 似乎mStack 永远只有一个变量来维持生活!
  std::vector<StackFrame> mStack;

  // 认为数组就是malloc
  // 函数重复执行，其实就是反复VisitStmt
  Heap mHeap;

  FunctionDecl *mFree; /// Declartions to the built-in functions
  FunctionDecl *mMalloc;
  FunctionDecl *mInput;
  FunctionDecl *mOutput;

  FunctionDecl *mEntry;

public:
  /// Get the declartions to the built-in functions
  Environment()
      : mStack(), mHeap(), mFree(NULL), mMalloc(NULL), mInput(NULL),
        mOutput(NULL), mEntry(NULL) {}

  void init(clang::ASTContext &Context);
  void callReturn(CallExpr *callexpr);
  Stmt *call(CallExpr *callexpr);
  void cast(CastExpr *castexpr);
  void declref(DeclRefExpr *declref);
  void intLiteral(IntegerLiteral *i);

  void decl(DeclStmt *declstmt, InterpreterVisitor *visitor);
  bool ifStmt(IfStmt *ifstmt);
  bool forStmt(ForStmt *f);
  bool whileStmt(WhileStmt *ws);
  void uop(UnaryOperator *uo);
  void array(ArraySubscriptExpr *array);
  void binop(BinaryOperator *bop);
  void typeTrait(UnaryExprOrTypeTraitExpr *tt);
  void returnStmt(ReturnStmt *ret);
  void parenExpr(ParenExpr * p);
  FunctionDecl *getEntry();
};
