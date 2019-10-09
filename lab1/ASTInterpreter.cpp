//==--- tools/clang-check/ClangInterpreter.cpp - Clang Interpreter tool
//--------------===//
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/EvaluatedExprVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"
#include <iostream>

using namespace clang;

#include "Environment.h"
// EvaluatedExprVisitor 当前的visitor 只能访问表达式
// The RecursiveASTVisitor provides hooks of the form bool
// VisitNodeType(NodeType *) for most AST nodes; the exception are TypeLoc
// nodes, which are passed by-value.
// TODO visitor 模式是如何工作的 ?
// TODO 如果才可以查询到clang 的词法规则和语法规则
#define DUMP(x)                                                                \
  if (false) {                                                                 \
    x->dump();                                                                 \
  }

/// Initialize the Environment
void Environment::init(clang::ASTContext &Context) {
  // 其余的全局变量一律不管，仅仅在乎实现申明的函数
  TranslationUnitDecl *unit = Context.getTranslationUnitDecl();
  unit->dump();
  auto gStatck = StackFrame();
  for (TranslationUnitDecl::decl_iterator i = unit->decls_begin(),
                                          e = unit->decls_end();
       i != e; ++i) {
    if (FunctionDecl *fdecl = dyn_cast<FunctionDecl>(*i)) {
      if (fdecl->getName().equals("FREE"))
        mFree = fdecl;
      else if (fdecl->getName().equals("MALLOC"))
        mMalloc = fdecl;
      else if (fdecl->getName().equals("GET"))
        mInput = fdecl;
      else if (fdecl->getName().equals("PRINT"))
        mOutput = fdecl;
      else if (fdecl->getName().equals("main"))
        mEntry = fdecl;

      // 其实没有处理全局变量的数组初始化!
    } else if (VarDecl *vardecl = dyn_cast<VarDecl>(*i)) {
      Expr *v = vardecl->getInit();
      if (v == nullptr) {
        gStatck.bindDecl(vardecl, 0);
        continue;
      }

      Expr::EvalResult Result;
      if (v->EvaluateAsInt(Result, Context)) {
        fprintf(stderr, "%s\n", "bind value : ");
        vardecl->dump();
        gStatck.bindDecl(vardecl, Result.Val.getInt().signedRoundToDouble());
      } else {
        gStatck.bindDecl(vardecl, 0); // 当前无法处理 int b; 的情况
        fprintf(stderr, "%s\n",
                "randomly copied from stackoverflow and I haven't check it");
        assert(false);
      }
    } else {
      // 似乎在此处处理预定义的东西 TODO 并不知道如何操纵其
    }
  }

  // 解释不在乎函数指针，但是在 implicit 转化的时候回出现问题!
  // TODO 似乎没有必要!
  // gStatck.bindStmt((Stmt *)mFree, 0x4396);
  // gStatck.bindStmt((Stmt *)mMalloc, 0x4396);
  // gStatck.bindStmt((Stmt *)mInput, 0x4396);
  // gStatck.bindStmt((Stmt *)mOutput, 0x4396);

  mStack.push_back(std::move(gStatck));
}

FunctionDecl *Environment::getEntry() { return mEntry; }

void Environment::returnStmt(ReturnStmt *ret) {
  auto val = mStack.back().getStmtVal(ret->getRetValue());
  mStack.back().setReturnVal(val);
}

void Environment::typeTrait(UnaryExprOrTypeTraitExpr *tt) {
  lld val;
  if (tt->getArgumentType()->isIntegerType()) {
    val = sizeof(int);
  } else if (tt->getArgumentType()->isPointerType()) {
    val = sizeof(void *);
  } else {
    TODO()
  }
  // tt->getArgumentType(); int 表示sizeof 中间的类型
  // tt->getType(); unsigned long 表示sizeof
  mStack.back().bindStmt(tt, val);
}

void Environment::parenExpr(ParenExpr *p) {
  // 直接拷贝的
  mStack.back().setPC(p);
  if (p->getType()->isIntegerType()) {
    Expr *expr = p->getSubExpr();
    lld val = mStack.back().getStmtVal(expr);
    mStack.back().bindStmt(p, val);
  }

  else if (p->getType()->isPointerType()) {
    if (p->getType()->isFunctionPointerType() == true) {
      // fprintf(stderr, "%s\n",
      // "Oh, paren come across a cute function pointer implicit cast, just "
      // "let it go !");
      return;
    }
    Expr *expr = p->getSubExpr();
    lld val = mStack.back().getStmtVal(expr);
    mStack.back().bindStmt(p, val);

    lld addr = mHeap.getStmtVal(expr);
    if (addr != 0)
      mHeap.bindStmt(p, addr);
  }

  else {
    fprintf(stderr, "%s\n", "wow, so how parent to handle this !");
    p->getType()->dump();
    p->dump();
    assert(false);
  }
}

/// !TODO Support comparison operation
void Environment::binop(BinaryOperator *bop) {
  Expr *left = bop->getLHS();
  Expr *right = bop->getRHS();

  if (bop->isAssignmentOp()) {
    lld val = mStack.back().getStmtVal(right);
    mStack.back().bindStmt(left, val);

    fprintf(stderr, "%s\n", "assignment debug :\nleft : ");
    left->dump();
    fprintf(stderr, "right : %lld\n", val);

    if (DeclRefExpr *declexpr = dyn_cast<DeclRefExpr>(left)) {
      // 如果是变量赋值，重置赋值
      Decl *decl = declexpr->getFoundDecl();
      mStack.back().bindDecl(decl, val);
    } else {
      lld addr = mHeap.getStmtVal(left);
      if (left->getType()->isIntegerType()) {
        fprintf(stderr, "%s %p %lld\n", "pointer & array assign value",
                (void *)addr, val);
        *((int *)addr) = val;
      } else if (left->getType()->isPointerType()) {
        *((lld *)addr) = val;
      } else {
        assert(false);
      }
    }

  } else if (bop->isAdditiveOp()) {
    lld lval = mStack.back().getStmtVal(left);
    lld rval = mStack.back().getStmtVal(right);

    lld size = 1;
    if (left->getType()->isPointerType()) {
      auto t = left->getType()->getPointeeType();
      if (t->isIntegerType()) {
        size = 4;
      } else if (t->isCharType()) {
      } else if (t->isPointerType()) {
        size = 8;
      } else {
        TODO()
      }
    }
    rval *= size;
    mStack.back().bindStmt(bop, lval + rval);

  } else if (bop->isMultiplicativeOp()) {
    lld rval = mStack.back().getStmtVal(right);
    lld lval = mStack.back().getStmtVal(left);
    mStack.back().bindStmt(bop, rval * lval);
  } else if (bop->isEqualityOp()) {
    lld rval = mStack.back().getStmtVal(right);
    lld lval = mStack.back().getStmtVal(left);
    mStack.back().bindStmt(bop, rval == lval ? 1 : 0);
  } else if (bop->isComparisonOp()) {
    lld rval = mStack.back().getStmtVal(right);
    lld lval = mStack.back().getStmtVal(left);
    lld res = 0;
    switch (bop->getOpcode()) {
    case BinaryOperator::Opcode::BO_GT:
      res = lval > rval ? 1 : 0;
      break;
    case BinaryOperator::Opcode::BO_GE:
      res = lval >= rval ? 1 : 0;
      break;
    case BinaryOperator::Opcode::BO_LE:
      res = lval <= rval ? 1 : 0;
      break;
    case BinaryOperator::Opcode::BO_LT:
      res = lval < rval ? 1 : 0;
      break;
    default:
      TODO()
    }
    mStack.back().bindStmt(bop, res);
  } else {
    bop->dump();
    TODO()
  }
}

void Environment::array(ArraySubscriptExpr *array) {
  // array 和 deref 一样，同样利用heap的支持
  printf("%s\n", "detect array : ");
  auto lhs = array->getBase();
  auto rhs = array->getIdx();

  lhs->dump();
  rhs->dump();

  // 如果不是指针类型，那么无法了解!
  auto t = lhs->getType()->getPointeeType();

  lld size; // 数据类型的长度
  if (t->isIntegerType()) {
    size = 4;
  } else if(t->isPointerType()) {
    size = 8;
  }else{
    TODO()
  }

  lld offset = mStack.back().getStmtVal(rhs);
  lld base = mStack.back().getStmtVal(lhs);
  fprintf(stderr, "array base : %p offset: %lld data size: %lld\n", (void *)base,
          offset, size);
  fprintf(stderr, "array address: %p\n", (void *)(base + offset));
  fprintf(stderr, "array value : %d\n", *((int *)(base + offset)));

  offset *= size;

  if (t->isIntegerType()) {
    mStack.back().bindStmt(array, *((int *)(base + offset)));
  }else if (t->isPointerType()) {
    mStack.back().bindStmt(array, (lld)*((int * *)(base + offset)));
  }else{
    TODO()
  }
  mHeap.bindStmt(array, base + offset);
}

void Environment::uop(UnaryOperator *uo) {
  Expr *e = uo->getSubExpr();
  lld val = mStack.back().getStmtVal(e);
  if (uo->getOpcode() == UnaryOperator::Opcode::UO_AddrOf) {
    TODO()
  } else if (uo->getOpcode() == UnaryOperator::Opcode::UO_Minus) {
    mStack.back().bindStmt(uo, -val);
  } else if (uo->getOpcode() == UnaryOperator::Opcode::UO_Deref) {
    // TODO 丑陋的hard code
    fprintf(stderr, "ana p: %p\n", (void *)val);
    if (uo->getType()->isIntegerType()) {
      // 4 对齐
      int *tmp = (int *)val;
      mStack.back().bindStmt(uo, *tmp);
      mHeap.bindStmt(uo, val);
    } else if (uo->getType()->isPointerType()) {
      // 8 对齐
      lld *tmp = (lld *)val;
      mStack.back().bindStmt(uo, *tmp);
      mHeap.bindStmt(uo, val);
    } else {
      assert(false);
    }
  } else {
    TODO()
  }
}

bool Environment::ifStmt(IfStmt *ifstmt) {
  // TODO 如果if 中间含有多个嵌套如何处理
  return mStack.back().getStmtVal(ifstmt->getCond()) == 1;
}

bool Environment::whileStmt(WhileStmt *ws) {
  return mStack.back().getStmtVal(ws->getCond()) == 1;
}

bool Environment::forStmt(ForStmt *f) {
  return mStack.back().getStmtVal(f->getCond()) == 1;
}

// 1. 支持一次申明多个变量
// 2. 变量初始值设置为0
void Environment::decl(DeclStmt *declstmt, InterpreterVisitor *visitor) {
  for (DeclStmt::decl_iterator it = declstmt->decl_begin(),
                               ie = declstmt->decl_end();
       it != ie; ++it) {
    Decl *decl = *it;
    if (VarDecl *vardecl = dyn_cast<VarDecl>(decl)) {
      // https://stackoverflow.com/questions/38516855/how-to-check-if-a-variable-declaration-in-clang-astvisitor-is-an-array
      // 如果申明的是数组，那么在此处malloc，然后在stackframe的时候free

      auto type = vardecl->getType();

      // if(type->isArrayType()) {
      // const ArrayType *Array = type->castAsArrayTypeUnsafe();
      // std::cout << "Is array of type: "  <<
      // Array->getElementType().getAsString() << std::endl; std::cout << "Is
      // array of type: "  << Array->getSizeModifier() << std::endl; std::cout
      // << vardecl->getType()->isVariableArrayType() << std::endl;
      // }

      // 默认初始值为 0
      lld val = 0;

      // TODO 并没有办法处理 int a[2] = {1, 2} 之类的数组初始化并且赋值的情况
      // 只有 int a[12] 这种类型
      // TODO 似乎无法处理二维数组
      if (auto t = dyn_cast_or_null<ConstantArrayType>(type.getTypePtr())) {
        // t->getSize().dump(); // We got the array size as an APInt here
        // t->getSizeExpr()->dump(); // 一般没有什么作用!
        lld size = (lld)t->getSize().signedRoundToDouble();
        auto et = t->getElementType();
        if (et->isIntegerType()) {
          val = (lld)malloc(sizeof(int) * size);
        } else if (et->isPointerType()) {
          val = (lld)malloc(sizeof(void *) * size);
        } else {
          TODO()
        }
      } else if (auto t =
                     dyn_cast_or_null<VariableArrayType>(type.getTypePtr())) {
        t->getSizeExpr()->dump();
        TODO() // 暂时不处理variableArray 的情况
      } else {
        Expr *e = vardecl->getInit();
        if (e == nullptr) {
          val = 0; // 当没有初始化部分的时候直接赋值 0
        } else {
          visitor->Visit(e); // !!
          val = mStack.back().getStmtVal(e);
        }
      }
      mStack.back().bindDecl(vardecl, val);
    }
  }
}

void Environment::intLiteral(IntegerLiteral *i) {
  mStack.back().bindStmt(i, i->getValue().signedRoundToDouble());
}

void Environment::declref(DeclRefExpr *declref) {
  mStack.back().setPC(declref);
  if (declref->getType()->isIntegerType() ||
      declref->getType()->isPointerType() ||
      declref->getType()->isArrayType()) {
    // ref 变量操作的操作: 从decl 装载到stmt 中间
    // 遇到print函数的时候也会询问，但是print并没有注入到decl的表中间
    // 但是实际上函数的体系是通过 getDirectCallee 的方法!
    Decl *decl = declref->getFoundDecl();
    lld val = mStack.back().getDeclVal(decl);
    mStack.back().bindStmt(declref, val);
  } else if (declref->getType()->isFunctionProtoType()) {
    // doing nothing !
  } else {
    declref->getType().dump();
    TODO()
  }
}

void Environment::cast(CastExpr *castexpr) {
  mStack.back().setPC(castexpr);
  if (castexpr->getType()->isIntegerType()) {
    Expr *expr = castexpr->getSubExpr();
    // 地址和普通的数值根本无法区分管理，而且地址变量本来也就是stack frame
    // 管理的东西
    lld val = mStack.back().getStmtVal(expr);
    mStack.back().bindStmt(castexpr, val);
  }

  else if (castexpr->getType()->isPointerType()) {
    // 函数指针类型的变化我们无需处理
    // 由于从 declref 的位置都是
    if (castexpr->getType()->isFunctionPointerType() == true) {
      // fprintf(stderr, "%s\n",
      // "Oh, come across a cute function pointer implicit cast, just "
      // "let it go !");
      return;
    }
    // https://stackoverflow.com/questions/25359555/how-to-get-the-arguments-of-a-function-pointer-from-a-callexpr-in-clang
    Expr *expr = castexpr->getSubExpr();
    lld val = mStack.back().getStmtVal(expr);
    mStack.back().bindStmt(castexpr, val);

    lld addr = mHeap.getStmtVal(expr);
    if (addr != 0)
      mHeap.bindStmt(castexpr, addr);
  }

  else {
    fprintf(stderr, "%s\n", "wow, so how to handle this !");
    castexpr->getType()->dump();
    castexpr->dump();
    assert(false);
  }
}

/// !TODO Support Function Call
Stmt *Environment::call(CallExpr *callexpr) {
  mStack.back().setPC(callexpr);
  lld val = 0;
  FunctionDecl *callee = callexpr->getDirectCallee();

  if (callee == mInput) {
    llvm::errs() << "Please Input an Integer Value : ";
    scanf("%lld", &val);
    mStack.back().bindStmt(callexpr, val);
  } else if (callee == mOutput) {
    Expr *decl = callexpr->getArg(0);
    val = mStack.back().getStmtVal(decl);
    llvm::errs() << val;
  } else if (callee == mFree) {
    Expr *decl = callexpr->getArg(0);
    val = mStack.back().getStmtVal(decl);
    free((void *)val);
  } else if (callee == mMalloc) {
    Expr *decl = callexpr->getArg(0);
    val = mStack.back().getStmtVal(decl);
    auto addr = malloc(val);
    mStack.back().bindStmt(callexpr, (lld)addr);
    fprintf(stderr, "ana  : %lld\n", (lld)addr);
    fprintf(stderr, "ana p: %p\n", addr);
  } else {
    StackFrame s;
    auto paraCount = callee->getNumParams();
    for (unsigned i = 0; i < paraCount; ++i) {
      Expr *decl = callexpr->getArg(i);
      val = mStack.back().getStmtVal(decl);
      s.bindDecl(callee->getParamDecl(i), val);
    }
    mStack.push_back(s);
    return callee->getBody();
  }
  return nullptr;
}

void Environment::callReturn(CallExpr *callexpr) {
  FunctionDecl *callee = callexpr->getDirectCallee();
  if (!callee->getReturnType()->isVoidType()) {
    lld t = mStack.back().getReturnVal();
    mStack.pop_back();
    mStack.back().bindStmt(callexpr, t);
  }
}

// 一共可以处理的类型 ?
void InterpreterVisitor::VisitBinaryOperator(BinaryOperator *bop) {
  DUMP(bop)

  // fprintf(stderr, "%s\n", "someone visit bop");
  bop->dump();

  VisitStmt(bop);
  mEnv->binop(bop);
}

void InterpreterVisitor::VisitUnaryOperator(UnaryOperator *uo) {
  DUMP(uo)
  VisitStmt(uo);
  mEnv->uop(uo);
}

void InterpreterVisitor::VisitDeclRefExpr(DeclRefExpr *expr) {
  DUMP(expr)
  VisitStmt(expr);
  mEnv->declref(expr);
}

void InterpreterVisitor::VisitUnaryExprOrTypeTraitExpr(
    UnaryExprOrTypeTraitExpr *tt) {
  DUMP(tt)
  mEnv->typeTrait(tt);
}

void InterpreterVisitor::VisitCastExpr(CastExpr *expr) {
  DUMP(expr)
  VisitStmt(expr);
  mEnv->cast(expr);
}

void InterpreterVisitor::VisitArraySubscriptExpr(ArraySubscriptExpr *array) {
  VisitStmt(array);
  mEnv->array(array);
}

void InterpreterVisitor::VisitCallExpr(CallExpr *call) {
  DUMP(call)
  VisitStmt(call);
  auto body = mEnv->call(call);
  if (body != nullptr) {
    VisitStmt(body);
    mEnv->callReturn(call);
  }
}

void InterpreterVisitor::VisitIntegerLiteral(IntegerLiteral *i) {
  DUMP(i)

  // fprintf(stderr, "%s\n", "someone visit Integer literal");
  mEnv->intLiteral(i);
}

void InterpreterVisitor::VisitReturnStmt(ReturnStmt *ret) {
  VisitStmt(ret);
  mEnv->returnStmt(ret);
}

void InterpreterVisitor::VisitIfStmt(IfStmt *ifstmt) {
  // https://clang.llvm.org/doxygen/classclang_1_1IfStmt.html

  // VisitStmt(ifstmt->getCond()) 是访问子节点的含义
  BinaryOperator *bop = dyn_cast<BinaryOperator>(ifstmt->getCond());
  assert(bop != NULL);
  VisitBinaryOperator(bop);
  // 需要假定总是if else 结构

  // #define GG(x, y) \
     // std::cout << y << std::endl;   \
     // if(ifstmt->get ## x()){       \
        // ifstmt->get ## x()->dump();\
     // }
  //
  // GG(Init, 0)
  // GG(Then, 1)
  // GG(Else, 2)

  // 虽然都是 Stmt，但是有的如果被直接转化为 BinaryOperator 那么就不能使用
  // VisitStmt 的方法执行了，和expr 不同的地方是什么
  if (mEnv->ifStmt(ifstmt)) {
    assert(ifstmt->getThen() != nullptr);
    this->Visit(ifstmt->getThen());
    // BinaryOperator *bop = dyn_cast<BinaryOperator>(ifstmt->getThen());
    // if (bop != NULL) {
    // VisitBinaryOperator(bop);
    // } else {
    // VisitStmt(ifstmt->getThen());
    // }
  } else {
    if (ifstmt->getElse() != nullptr) {
      this->Visit(ifstmt->getElse());
      // BinaryOperator *bop = dyn_cast<BinaryOperator>(ifstmt->getElse());
      // if (bop != NULL) {
      // VisitBinaryOperator(bop);
      // } else {
      // VisitStmt(ifstmt->getElse());
      // }
    }
  }
}

void InterpreterVisitor::VisitWhileStmt(WhileStmt *ws) {
  BinaryOperator *bop = dyn_cast<BinaryOperator>(ws->getCond());
  assert(bop != NULL);

  VisitBinaryOperator(bop);
  while (mEnv->whileStmt(ws)) {
    VisitStmt(ws->getBody());
    VisitBinaryOperator(bop);
  }
}

void InterpreterVisitor::VisitForStmt(ForStmt *f) {
  BinaryOperator *bop = dyn_cast<BinaryOperator>(f->getCond());
  assert(bop != NULL);
  BinaryOperator *inc = dyn_cast<BinaryOperator>(f->getInc());
  assert(inc != NULL);

  if (f->getInit() != nullptr) {
    VisitStmt(f->getInit());
  }

  // 其实只有stmt 才可以VisitStmt 啊!
  // fprintf(stderr, "%s\n", "life is hard!");
  // f->getInit()->dump();
  // f->getBody()->dump();
  // f->getInc()->dump();
  // f->getCond()->dump();
  // fprintf(stderr, "%s\n", "life is hard!");

  VisitBinaryOperator(bop);
  while (mEnv->forStmt(f)) {
    VisitStmt(f->getBody());
    VisitBinaryOperator(inc);
    VisitBinaryOperator(bop);
  }
}

void InterpreterVisitor::VisitDeclStmt(DeclStmt *declstmt) {
  DUMP(declstmt)
  // VisitStmt(declstmt);
  mEnv->decl(declstmt, this);
}

void InterpreterVisitor::VisitParenExpr(ParenExpr *p) {
  // this->Visit(p); // TODO 根本没有搞清楚Visit的适用范围!
  VisitStmt(p);
  mEnv->parenExpr(p);
}

// expr and stmt are different !
// void InterpreterVisitor::VisitVarDecl(VarDecl *vardecl){
// VisitStmt(vardecl);
// }

// ASTConsumer is an interface used to write generic actions on an AST,
// regardless of how the AST was produced. ASTConsumer provides many different
// entry points, but for our use case the only one needed is
// HandleTranslationUnit, which is called with the ASTContext for the
// translation unit.
class InterpreterConsumer : public ASTConsumer {
public:
  explicit InterpreterConsumer(const ASTContext &context)
      : mEnv(), mVisitor(context, &mEnv) {}
  ~InterpreterConsumer() {}

  void HandleTranslationUnit(clang::ASTContext &Context) {
    // TODO 获取的到底是什么东西 ?
    mEnv.init(Context);

    FunctionDecl *entry = mEnv.getEntry();
    // 似乎说明必须含有必须含有main 函数
    assert(entry != NULL);
    mVisitor.VisitStmt(
        entry->getBody()); // 从main
                           // 函数中间开始分析，但是此时已经持有各种申明信息
  }

private:
  Environment mEnv;
  InterpreterVisitor mVisitor;
};

// Compiler.getASTContext() 中间的context 的作用是什么 ?
//
// Some of the information about the AST,
// like source locations and global identifier information,
// are not stored in the AST nodes themselves,
// but in the ASTContext and its associated source manager.
// To retrieve them we need to hand the ASTContext into our RecursiveASTVisitor
// implementation.
//
// FrontendAction is an interface that allows execution of user specific actions
// as part of the compilation.
class InterpreterClassAction : public ASTFrontendAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &Compiler, llvm::StringRef InFile) {
    return std::unique_ptr<clang::ASTConsumer>(
        new InterpreterConsumer(Compiler.getASTContext()));
  }
};

int main(int argc, char **argv) {
  if (argc == 2) {
    clang::tooling::runToolOnCode(
        clang::tooling::newFrontendActionFactory<InterpreterClassAction>()
            ->create(),
        argv[1]);
  } else {
    std::cout << "usage :\n\t./lab1 input.cc " << std::endl;
  }
}
