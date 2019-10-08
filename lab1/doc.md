
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



Program received signal SIGABRT, Aborted.
0x00007ffff62a0755 in raise () from /usr/lib/libc.so.6
(gdb) backtrace 
#0  0x00007ffff62a0755 in raise () from /usr/lib/libc.so.6
#1  0x00007ffff628b851 in abort () from /usr/lib/libc.so.6
#2  0x00007ffff628b727 in __assert_fail_base.cold () from /usr/lib/libc.so.6
#3  0x00007ffff6299026 in __assert_fail () from /usr/lib/libc.so.6
#4  0x000055555589bc8d in StackFrame::getStmtVal (this=0x55555906d020, stmt=0x555559068cc8)
    at /run/media/shen/Will/Compiler/llvm-project/clang-tools-extra/lab1/Environment.h:53
#5  0x000055555589cd24 in Environment::cast (this=0x55555901dd70, castexpr=0x555559068d18)
    at /run/media/shen/Will/Compiler/llvm-project/clang-tools-extra/lab1/Environment.h:236
#6  0x000055555589d242 in InterpreterVisitor::VisitCastExpr (this=0x55555901ddb0, expr=0x555559068d18)
    at /run/media/shen/Will/Compiler/llvm-project/clang-tools-extra/lab1/ASTInterpreter.cpp:61
#7  0x00005555558a4b62 in clang::StmtVisitorBase<std::add_pointer, InterpreterVisitor, void>::VisitImplicitCastExpr (
    this=0x55555901ddb8, S=0x555559068d18) at tools/clang/include/clang/AST/StmtNodes.inc:865
#8  0x00005555558a0aac in clang::StmtVisitorBase<std::add_pointer, InterpreterVisitor, void>::Visit (
    this=0x55555901ddb8, S=0x555559068d18) at tools/clang/include/clang/AST/StmtNodes.inc:865
#9  0x000055555589e4bb in clang::EvaluatedExprVisitorBase<std::add_pointer, InterpreterVisitor>::VisitStmt (
    this=0x55555901ddb8, S=0x555559068d30)
    at /run/media/shen/Will/Compiler/llvm-project/clang/include/clang/AST/EvaluatedExprVisitor.h:103
#10 0x000055555589d26d in InterpreterVisitor::VisitCallExpr (this=0x55555901ddb0, call=0x555559068d30)
    at /run/media/shen/Will/Compiler/llvm-project/clang-tools-extra/lab1/ASTInterpreter.cpp:66
#11 0x00005555558a0974 in clang::StmtVisitorBase<std::add_pointer, InterpreterVisitor, void>::Visit (
    this=0x55555901ddb8, S=0x555559068d30) at tools/clang/include/clang/AST/StmtNodes.inc:765
#12 0x000055555589e4bb in clang::EvaluatedExprVisitorBase<std::add_pointer, InterpreterVisitor>::VisitStmt (
    this=0x55555901ddb8, S=0x555559068d88)
    at /run/media/shen/Will/Compiler/llvm-project/clang/include/clang/AST/EvaluatedExprVisitor.h:103
#13 0x000055555589d22b in InterpreterVisitor::VisitCastExpr (this=0x55555901ddb0, expr=0x555559068d88)
    at /run/media/shen/Will/Compiler/llvm-project/clang-tools-extra/lab1/ASTInterpreter.cpp:60
#14 0x00005555558a69a8 in clang::StmtVisitorBase<std::add_pointer, InterpreterVisitor, void>::VisitExplicitCastExpr (
    this=0x55555901ddb8, S=0x555559068d88) at tools/clang/include/clang/AST/StmtNodes.inc:801
#15 0x00005555558a4a45 in clang::StmtVisitorBase<std::add_pointer, InterpreterVisitor, void>::VisitCStyleCastExpr (
    this=0x55555901ddb8, S=0x555559068d88) at tools/clang/include/clang/AST/StmtNodes.inc:811
#16 0x00005555558a0a04 in clang::StmtVisitorBase<std::add_pointer, InterpreterVisitor, void>::Visit (
    this=0x55555901ddb8, S=0x555559068d88) at tools/clang/include/clang/AST/StmtNodes.inc:811
#17 0x000055555589e4bb in clang::EvaluatedExprVisitorBase<std::add_pointer, InterpreterVisitor>::VisitStmt (
    this=0x55555901ddb8, S=0x555559068db0)
    at /run/media/shen/Will/Compiler/llvm-project/clang/include/clang/AST/EvaluatedExprVisitor.h:103
#18 0x000055555589d0f1 in InterpreterVisitor::VisitBinaryOperator (this=0x55555901ddb0, bop=0x555559068db0)
    at /run/media/shen/Will/Compiler/llvm-project/clang-tools-extra/lab1/ASTInterpreter.cpp:34
--Type <RET> for more, q to quit, c to continue without paging--


// 并不清楚当前的错误是否可以通过分析
➜  build git:(master) ✗ ./run.sh a
[2/2] Linking CXX executable bin/lab1
TranslationUnitDecl 0x559609c25e68 <<invalid sloc>> <invalid sloc>
|-TypedefDecl 0x559609c26740 <<invalid sloc>> <invalid sloc> implicit __int128_t '__int128'
| `-BuiltinType 0x559609c26400 '__int128'
|-TypedefDecl 0x559609c267b0 <<invalid sloc>> <invalid sloc> implicit __uint128_t 'unsigned __int128'
| `-BuiltinType 0x559609c26420 'unsigned __int128'
|-TypedefDecl 0x559609c26b28 <<invalid sloc>> <invalid sloc> implicit __NSConstantString '__NSConstantString_tag'
| `-RecordType 0x559609c268a0 '__NSConstantString_tag'
|   `-CXXRecord 0x559609c26808 '__NSConstantString_tag'
|-TypedefDecl 0x559609c26bc0 <<invalid sloc>> <invalid sloc> implicit __builtin_ms_va_list 'char *'
| `-PointerType 0x559609c26b80 'char *'
|   `-BuiltinType 0x559609c25f00 'char'
|-TypedefDecl 0x559609c63348 <<invalid sloc>> <invalid sloc> implicit __builtin_va_list '__va_list_tag [1]'
| `-ConstantArrayType 0x559609c632f0 '__va_list_tag [1]' 1 
|   `-RecordType 0x559609c26cb0 '__va_list_tag'
|     `-CXXRecord 0x559609c26c18 '__va_list_tag'
|-FunctionDecl 0x559609c633f0 <input.cc:1:1, col:16> col:12 GET 'int ()' extern
|-FunctionDecl 0x559609c635c0 <line:2:1, col:25> col:15 used MALLOC 'void *(int)' extern
| `-ParmVarDecl 0x559609c634f0 <col:22> col:25 'int'
|-FunctionDecl 0x559609c63748 <line:3:1, col:24> col:13 used FREE 'void (void *)' extern
| `-ParmVarDecl 0x559609c63680 <col:18, col:23> col:24 'void *'
|-FunctionDecl 0x559609c638c8 <line:4:1, col:22> col:13 used PRINT 'void (int)' extern
| `-ParmVarDecl 0x559609c63808 <col:19> col:22 'int'
`-FunctionDecl 0x559609c63998 <line:7:1, line:16:1> line:7:5 main 'int ()'
  `-CompoundStmt 0x559609c640d8 <col:12, line:16:1>
    |-DeclStmt 0x559609c63b18 <line:8:4, col:10>
    | `-VarDecl 0x559609c63ab0 <col:4, col:9> col:9 used a 'int *'
    |-DeclStmt 0x559609c63bb0 <line:9:4, col:9>
    | `-VarDecl 0x559609c63b48 <col:4, col:8> col:8 used b 'int'
    |-BinaryOperator 0x559609c63c08 <line:10:4, col:8> 'int' lvalue '='
    | |-DeclRefExpr 0x559609c63bc8 <col:4> 'int' lvalue Var 0x559609c63b48 'b' 'int'
    | `-IntegerLiteral 0x559609c63be8 <col:8> 'int' 10
    |-BinaryOperator 0x559609c63db0 <line:12:4, col:43> 'int *' lvalue '='
    | |-DeclRefExpr 0x559609c63c28 <col:4> 'int *' lvalue Var 0x559609c63ab0 'a' 'int *'
    | `-CStyleCastExpr 0x559609c63d88 <col:8, col:43> 'int *' <BitCast>
    |   `-CallExpr 0x559609c63d30 <col:15, col:43> 'void *'
    |     |-ImplicitCastExpr 0x559609c63d18 <col:15> 'void *(*)(int)' <FunctionToPointerDecay>
    |     | `-DeclRefExpr 0x559609c63cc8 <col:15> 'void *(int)' lvalue Function 0x559609c635c0 'MALLOC' 'void *(int)'
    |     `-ImplicitCastExpr 0x559609c63d58 <col:22, col:42> 'int' <IntegralCast>
    |       `-UnaryExprOrTypeTraitExpr 0x559609c63ca8 <col:22, col:42> 'unsigned long' sizeof 'long long'
    |-BinaryOperator 0x559609c63e58 <line:13:4, col:9> 'int' lvalue '='
    | |-UnaryOperator 0x559609c63e08 <col:4, col:5> 'int' lvalue prefix '*' cannot overflow
    | | `-ImplicitCastExpr 0x559609c63df0 <col:5> 'int *' <LValueToRValue>
    | |   `-DeclRefExpr 0x559609c63dd0 <col:5> 'int *' lvalue Var 0x559609c63ab0 'a' 'int *'
    | `-ImplicitCastExpr 0x559609c63e40 <col:9> 'int' <LValueToRValue>
    |   `-DeclRefExpr 0x559609c63e20 <col:9> 'int' lvalue Var 0x559609c63b48 'b' 'int'
    |-CallExpr 0x559609c63f70 <line:14:4, col:12> 'void'
    | |-ImplicitCastExpr 0x559609c63f58 <col:4> 'void (*)(int)' <FunctionToPointerDecay>
    | | `-DeclRefExpr 0x559609c63f10 <col:4> 'void (int)' lvalue Function 0x559609c638c8 'PRINT' 'void (int)'
    | `-ImplicitCastExpr 0x559609c63f98 <col:10, col:11> 'int' <LValueToRValue>
    |   `-UnaryOperator 0x559609c63ef8 <col:10, col:11> 'int' lvalue prefix '*' cannot overflow
    |     `-ImplicitCastExpr 0x559609c63ee0 <col:11> 'int *' <LValueToRValue>
    |       `-DeclRefExpr 0x559609c63ec0 <col:11> 'int *' lvalue Var 0x559609c63ab0 'a' 'int *'
    `-CallExpr 0x559609c64080 <line:15:4, col:10> 'void'
      |-ImplicitCastExpr 0x559609c64068 <col:4> 'void (*)(void *)' <FunctionToPointerDecay>
      | `-DeclRefExpr 0x559609c64018 <col:4> 'void (void *)' lvalue Function 0x559609c63748 'FREE' 'void (void *)'
      `-ImplicitCastExpr 0x559609c640c0 <col:9> 'void *' <BitCast>
        `-ImplicitCastExpr 0x559609c640a8 <col:9> 'int *' <LValueToRValue>
          `-DeclRefExpr 0x559609c63ff8 <col:9> 'int *' lvalue Var 0x559609c63ab0 'a' 'int *'
bind value 0
bind value 0
bind value 10
ana :ana : 4
bind value 94102897091568
I am here
shit! we can't the find the stmt :
ImplicitCastExpr 0x559609c63df0 'int *' <LValueToRValue>
`-DeclRefExpr 0x559609c63dd0 'int *' lvalue Var 0x559609c63ab0 'a' 'int *'
lab1: /run/media/shen/Will/Compiler/llvm-project/clang-tools-extra/lab1/Environment.h:53: lld StackFrame::getStmtVal(clang::Stmt*): Assertion `mExprs.find(stmt) != mExprs.end()' failed.
./run.sh: line 12: 15033 Aborted   

# 根本不知道 location 的作用是什么啊!
