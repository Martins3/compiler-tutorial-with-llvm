
# question
1. ast consumer based 

https://clang.llvm.org/docs/LibASTMatchersTutorial.html
> 搭建环境

https://clang.llvm.org/docs/RAVFrontendAction.html
> this is it !

```cpp
Requirements: Implement a basic interpreter based on Clang

Marking: 25 testcases are provided and each test case counts for 1 mark

Supported Language: We support a subset of C language constructs, as follows: 

Type: int | char | void | *
Operator: * | + | - | * | / | < | > | == | = | [ ] 
Statements: IfStmt | WhileStmt | ForStmt | DeclStmt 
Expr : BinaryOperator | UnaryOperator | DeclRefExpr | CallExpr | CastExpr 

We also need to support 4 external functions int GET(), void * MALLOC(int), void FREE (void *), void PRINT(int), the semantics of the 4 funcions are self-explanatory. 

A skelton implemnentation ast-interpreter.tgz is provided, and you are welcome to make any changes to the implementation. The provided implementation is able to interpreter the simple program like : 

extern int GET();
extern void * MALLOC(int);
extern void FREE(void *);
extern void PRINT(int);

int main() {
   int a;
   a = GET();
   PRINT(a);
}

We provide a more formal definition of the accepted language, as follows: 


Program : DeclList
DeclList : Declaration DeclList | empty
Declaration : VarDecl FuncDecl
VarDecl : Type VarList;
Type : BaseType | QualType
BaseType : int | char | void
QualType : Type * 
VarList : ID, VarList |  | ID[num], VarList | emtpy
FuncDecl : ExtFuncDecl | FuncDefinition
ExtFuncDecl : extern int GET(); | extern void * MALLOC(int); | extern void FREE(void *); | extern void PRINT(int);
FuncDefinition : Type ID (ParamList) { StmtList }
ParamList : Param, ParamList | empty
Param : Type ID
StmtList : Stmt, StmtList | empty
Stmt : IfStmt | WhileStmt | ForStmt | DeclStmt | CompoundStmt | CallStmt | AssignStmt | 
IfStmt : if (Expr) Stmt | if (Expr) Stmt else Stmt
WhileStmt : while (Expr) Stmt
DeclStmt : Type VarList;
AssignStmt : DeclRefExpr = Expr;
CallStmt : CallExpr;
CompoundStmt : { StmtList }
ForStmt : for ( Expr; Expr; Expr) Stmt
Expr : BinaryExpr | UnaryExpr | DeclRefExpr | CallExpr | CastExpr | ArrayExpr | DerefExpr | (Expr) | num
BinaryExpr : Expr BinOP Expr
BinaryOP : + | - | * | / | < | > | ==
UnaryExpr : - Expr
DeclRefExpr : ID
CallExpr : DeclRefExpr (ExprList)
ExprList : Expr, ExprList | empty
CastExpr : (Type) Expr
ArrayExpr : DeclRefExpr [Expr]
DerefExpr : * DeclRefExpr
```

./ast-interpreter "`cat ./test/test_file_name`"

# 阅读文档
https://jonasdevlieghere.com/understanding-the-clang-ast/


https://clang.llvm.org/docs/IntroductionToTheClangAST.html

```
clang -Xclang -ast-dump -fsyntax-only test.cc
```

All information about the AST for a translation unit is bundled up in the class ASTContext. It allows traversal of the whole translation unit starting from getTranslationUnitDecl, or to access Clang’s **table of identifiers** for the parsed translation unit.

1 获取其中


# 01
```
|-VarDecl 0x55fcf9701988 <line:6:1, col:7> col:5 used b 'int' cinit
| `-IntegerLiteral 0x55fcf97019f0 <col:7> 'int' 10
```
在捕获函数的时候同时也捕获全局变量

## 02
> 支持其他的binary op 即可!
```
shit! we can't the find the stmt :
BinaryOperator 0x563b8eb9ed10 'int' '+'
|-ImplicitCastExpr 0x563b8eb9ece0 'int' <LValueToRValue>
| `-DeclRefExpr 0x563b8eb9eca0 'int' lvalue Var 0x563b8eb9ea80 'a' 'int'
`-ImplicitCastExpr 0x563b8eb9ecf8 'int' <LValueToRValue>
  `-DeclRefExpr 0x563b8eb9ecc0 'int' lvalue Var 0x563b8eb9eb18 'b' 'int'
```

## 03
1. 处理 if 的方法
2. ==


# 根本不知道 location 的作用是什么啊!



# todo
1. if 语句的实现，让人重新思考 visitor 模式的含义是什么啊 ?
    1. if 的问题，显然可以扩展到for 

2. int a = 12; 为什么出现错误 ?
3. for 中间没有赋值 ? assign 有区别?

