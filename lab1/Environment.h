//==--- tools/clang-check/ClangInterpreter.cpp - Clang Interpreter tool
//--------------===//
//===----------------------------------------------------------------------===//
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Decl.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"
#include <iostream>


class InterpreterVisitor;


#define TODO()                                                                 \
  fprintf(stderr, "TODO \n");                                                  \
  assert(false);

using namespace clang;

// TODO 为什么我们需要stack，从上到下逐个分析 ?
// 函数没有办法调用吗 ? 目前的怀疑是，外部的函数并不会被捕获!
// TMD 我哭了!
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
/// TODO 辅助处理越界问题
/// 通过lld 实现存储地址 ?

// 并没有办法逃脱使用heap
// 1. unary op 上传地址还是数值
// 2. 全部选择上传数值
// 3. 当需要地址的时候，从heap 中间访问
// 4. 在逐级构建expr : *(a + 4)
//    5. wait ! 实际上，只有在 unary
//    的时候开始检查，然后在连续的变化中间，始终维持heap 和 地址
//    6. *(a + 4) 的记录 a + 4 为地址和 *(a +4) 为值向上传递
//    7. *(a + 4) + 4  直接导致清空当前的 heap 记录表，其实根本不需要一个表格 ?
//    其实只需要一个变量即可!
//    8. 遇到 * 的时候建立刷新 address 的
//    9. *a = *a + 1
//    会出现问题，右侧的递归处理会掩盖左侧的内容，所以还是最好建立一个映射表
// 5. 只有cast 和初始化的时候 的时候处理添加
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

  ///
  /// Initialize the Environment
  void init(clang::ASTContext &Context) {
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

  FunctionDecl *getEntry() { return mEntry; }

  void returnStmt(ReturnStmt *ret) {
    auto val = mStack.back().getStmtVal(ret->getRetValue());
    mStack.back().setReturnVal(val);
  }

  void typeTrait(UnaryExprOrTypeTraitExpr *tt) {
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

  /// !TODO Support comparison operation
  void binop(BinaryOperator *bop) {
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
      lld rval = mStack.back().getStmtVal(right);
      lld lval = mStack.back().getStmtVal(left);
      mStack.back().bindStmt(bop, rval + lval);
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
      fprintf(stderr, "%s\n", "binary op Not implemented yet !");
      assert(false);
    }
  }

  void array(ArraySubscriptExpr *array) {
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
    } else {
      TODO()
    }

    lld offset = mStack.back().getStmtVal(rhs);
    lld base = mStack.back().getStmtVal(lhs);
    fprintf(stderr, "array offset: %lld data size: %lld\n", offset, size);
    fprintf(stderr, "array address: %p\n", (void *)(base + offset));
    fprintf(stderr, "array value : %d\n", *((int *)(base + offset)));

    offset *= size;

    if (t->isIntegerType()) {
      mStack.back().bindStmt(array, *((int *)(base + offset)));
    }
    mHeap.bindStmt(array, base + offset);
  }

  void uop(UnaryOperator *uo) {
    Expr *e = uo->getSubExpr();
    lld val = mStack.back().getStmtVal(e);
    if (uo->getOpcode() == UnaryOperator::Opcode::UO_AddrOf) {
      TODO()
    } else if (uo->getOpcode() == UnaryOperator::Opcode::UO_Minus) {
      mStack.back().bindStmt(uo, -val);
    } else if (uo->getOpcode() == UnaryOperator::Opcode::UO_Deref) {
      // TODO 丑陋的hard code
      fprintf(stderr, "ana  : %lld\n", val);
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

  bool ifStmt(IfStmt *ifstmt) {
    // TODO 如果if 中间含有多个嵌套如何处理
    return mStack.back().getStmtVal(ifstmt->getCond()) == 1;
  }

  bool whileStmt(WhileStmt *ws) {
    return mStack.back().getStmtVal(ws->getCond()) == 1;
  }

  bool forStmt(ForStmt *f) {
    return mStack.back().getStmtVal(f->getCond()) == 1;
  }

  // 1. 支持一次申明多个变量
  // 2. 变量初始值设置为0
  void decl(DeclStmt *declstmt, InterpreterVisitor * visitor) {
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
        if (auto t = dyn_cast_or_null<ConstantArrayType>(type.getTypePtr())) {
          // t->getSize().dump(); // We got the array size as an APInt here
          // t->getSizeExpr()->dump(); // 一般没有什么作用!
          lld size = (lld)t->getSize().signedRoundToDouble();
          auto et = t->getElementType();
          if (et->isIntegerType()) {
            val = (lld)malloc(sizeof(int) * size);
          } else {
            TODO()
          }
        } else if (auto t =
                       dyn_cast_or_null<VariableArrayType>(type.getTypePtr())) {
          t->getSizeExpr()->dump();
          TODO() // 暂时不处理variableArray 的情况
        }

        Expr *v = vardecl->getInit();
        if (v == nullptr) {
          // 当没有初始化部分的时候直接赋值 0
          val = 0;
        }else {
          // 对于变量求值，显然需要上传到visit 中间才可以
          // visitor->woo();
        }

        mStack.back().bindDecl(vardecl, val);
      }
    }
  }

  void intLiteral(IntegerLiteral *i) {
    mStack.back().bindStmt(i, i->getValue().signedRoundToDouble());
  }

  void declref(DeclRefExpr *declref) {
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

  void cast(CastExpr *castexpr) {
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
        fprintf(stderr, "%s\n",
                "Oh, come across a cute function pointer implicit cast, just "
                "let it go !");
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

  // 丑陋的写法 TODO
  // void cStyleCast(CStyleCastExpr * castexpr){
  // if (castexpr->getType()->isPointerType()){
  // Expr * expr = castexpr->getSubExpr();
  // lld val = mStack.back().getStmtVal(expr);
  // mStack.back().bindStmt(castexpr, val );
  // }else {
  // TODO()
  // }
  // }

  /// !TODO Support Function Call
  Stmt *call(CallExpr *callexpr) {
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

  void callReturn(CallExpr *callexpr) {
    FunctionDecl *callee = callexpr->getDirectCallee();
    if (!callee->getReturnType()->isVoidType()) {
      lld t = mStack.back().getReturnVal();
      mStack.pop_back();
      mStack.back().bindStmt(callexpr, t);
    }
  }
};

#define INFO(msg)                                                              \
  fprintf(stderr, "info: %s:%d: ", __FILE__, __LINE__);                        \
  fprintf(stderr, "%s\n", msg);
