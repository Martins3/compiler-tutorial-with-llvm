//==--- tools/clang-check/ClangInterpreter.cpp - Clang Interpreter tool --------------===//
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
// The RecursiveASTVisitor provides hooks of the form bool VisitNodeType(NodeType *) for most AST nodes; the exception are TypeLoc nodes, which are passed by-value.
// TODO visitor 模式是如何工作的 ?
// TODO 如果才可以查询到clang 的词法规则和语法规则
// TODO 下面的所有函数都应该override的
#define DUMP(x) \
  if(false){     \
    x->dump();  \
  }

class InterpreterVisitor : 
   public EvaluatedExprVisitor<InterpreterVisitor> {
public:
   explicit InterpreterVisitor(const ASTContext &context, Environment * env)
   : EvaluatedExprVisitor(context), mEnv(env) {}
   virtual ~InterpreterVisitor() {}

   // 一共可以处理的类型 ?
   virtual void VisitBinaryOperator (BinaryOperator * bop) {
     DUMP(bop)
     VisitStmt(bop);
     mEnv->binop(bop);
   }
   
   virtual void VisitUnaryOperator(UnaryOperator * uo){
     DUMP(uo)
     VisitStmt(uo);
     mEnv->uop(uo);
   }

   virtual void VisitDeclRefExpr(DeclRefExpr * expr){
     DUMP(expr)
	   VisitStmt(expr);
	   mEnv->declref(expr);
   }

   virtual void VisitUnaryExprOrTypeTraitExpr(UnaryExprOrTypeTraitExpr * tt){
     DUMP(tt)
	   mEnv->typeTrait(tt);
   }
   
   virtual void VisitCastExpr(CastExpr * expr) {
     DUMP(expr)
	   VisitStmt(expr);
	   mEnv->cast(expr);
   }

   virtual void VisitArraySubscriptExpr(ArraySubscriptExpr * array){
     VisitStmt(array);
     mEnv->array(array);
   }

   // 处理 (void *)int 之类转化
   // virtual void VisitCStyleCastExpr(CStyleCastExpr * cc){
     // DUMP(cc)
     // VisitStmt(cc);
     // mEnv->cStyleCast(cc);
   // }
   //
   void woo(){
     assert(false);
   }

   virtual void VisitCallExpr(CallExpr * call) {
     DUMP(call)
	   VisitStmt(call);
	   auto body = mEnv->call(call);
     if (body != nullptr) {
       VisitStmt(body);
       mEnv->callReturn(call);
     }
   }
  
   virtual void VisitIntegerLiteral(IntegerLiteral * i) {
     // 实际上，只要想添加，可以添加任何
     DUMP(i)
	   VisitStmt(i);
	   mEnv->intLiteral(i);
   }

   virtual void VisitReturnStmt(ReturnStmt * ret){
	   VisitStmt(ret);
	   mEnv->returnStmt(ret);
   }

   virtual void VisitIfStmt(IfStmt * ifstmt){
     // https://clang.llvm.org/doxygen/classclang_1_1IfStmt.html
     
     // VisitStmt(ifstmt->getCond()) 是访问子节点的含义
     BinaryOperator * bop = dyn_cast<BinaryOperator>(ifstmt->getCond());
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
     // TODO 使用lamda 改写一下!
     if (mEnv->ifStmt(ifstmt)){
       assert(ifstmt->getThen() != nullptr);
       BinaryOperator * bop = dyn_cast<BinaryOperator>(ifstmt->getThen());
       if(bop != NULL){
         VisitBinaryOperator(bop);
       }else{
         VisitStmt(ifstmt->getThen());
       }

     }else{
       if(ifstmt->getElse() != nullptr){
         BinaryOperator * bop = dyn_cast<BinaryOperator>(ifstmt->getElse());
         if(bop != NULL){
           VisitBinaryOperator(bop);
         }else{
           VisitStmt(ifstmt->getElse());
         }
       }
     }
   }

   virtual void VisitWhileStmt(WhileStmt * ws){
     BinaryOperator * bop = dyn_cast<BinaryOperator>(ws->getCond());
     assert(bop != NULL);

     VisitBinaryOperator(bop);
     while(mEnv->whileStmt(ws)){
      VisitStmt(ws->getBody());
      VisitBinaryOperator(bop);
     }
   }

   virtual void VisitForStmt(ForStmt * f){
     BinaryOperator * bop = dyn_cast<BinaryOperator>(f->getCond());
     assert(bop != NULL);
     BinaryOperator * inc = dyn_cast<BinaryOperator>(f->getInc());
     assert(inc != NULL);

     VisitStmt(f->getInit());

     

     // 其实只有stmt 才可以VisitStmt 啊!
     // fprintf(stderr, "%s\n", "life is hard!");
     // f->getInit()->dump();
     // f->getBody()->dump();
     // f->getInc()->dump();
     // f->getCond()->dump();
     // fprintf(stderr, "%s\n", "life is hard!");

     VisitBinaryOperator(bop);
     while(mEnv->forStmt(f)){
      VisitStmt(f->getBody());
      VisitBinaryOperator(inc);
      VisitBinaryOperator(bop);
     }
   }


   virtual void VisitDeclStmt(DeclStmt * declstmt) {
     DUMP(declstmt)
	   mEnv->decl(declstmt, this);
   }
private:
   Environment * mEnv;
};

// ASTConsumer is an interface used to write generic actions on an AST,
// regardless of how the AST was produced. ASTConsumer provides many different entry points,
// but for our use case the only one needed is HandleTranslationUnit, which is called with the ASTContext for the translation unit.
class InterpreterConsumer : public ASTConsumer {
public:
   explicit InterpreterConsumer(const ASTContext& context) : mEnv(),
   	   mVisitor(context, &mEnv) {
   }
   virtual ~InterpreterConsumer() {}

   virtual void HandleTranslationUnit(clang::ASTContext &Context) {
     // TODO 获取的到底是什么东西 ?
	   mEnv.init(Context);

	   FunctionDecl * entry = mEnv.getEntry();
     // 似乎说明必须含有必须含有main 函数
     assert(entry != NULL);
	   mVisitor.VisitStmt(entry->getBody()); // 从main 函数中间开始分析，但是此时已经持有各种申明信息
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
// To retrieve them we need to hand the ASTContext into our RecursiveASTVisitor implementation.
//
// FrontendAction is an interface that allows execution of user specific actions as part of the compilation.
class InterpreterClassAction : public ASTFrontendAction {
public: 
  virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
    clang::CompilerInstance &Compiler, llvm::StringRef InFile) {
    return std::unique_ptr<clang::ASTConsumer>(
        new InterpreterConsumer(Compiler.getASTContext()));
  }
};

int main (int argc, char ** argv) {
   if (argc == 2) {
       clang::tooling::runToolOnCode(clang::tooling::newFrontendActionFactory<InterpreterClassAction>()->create(), argv[1]);
   }else{
     std::cout << "usage :\n\t./lab1 input.cc " << std::endl;
   }
}
