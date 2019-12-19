# Wow
You are supposed to implement a flow-sensitive, field- and context-insensitive algorithm to compute the 
points-to set for each variable at each distinct program point.
One approach is to extend the provided dataflow analysis framework as follows: 
- Implement your representation of points-to sets
- Implement the transfer function for each instruction
- Implement the MEET operator
- Extend the control flow graph to be inter-procedural


## Read the code framework
https://www.cs.cmu.edu/~aldrich/courses/15-819O-13sp/resources/pointer.pdf
https://www.cs.utexas.edu/users/lin/papers/cgo11.pdf

**https://llvm.org/docs/TypeMetadata.html**

All the time, we are handling the same problem,
what if one of some "crash", the answer is clear :
after this instruction, the dfval is cleaned !
1. generally, the block is called with correct data flow, 
but the algorithm works even the dataflow is random,
by clean the dfval

- point to relation : not only function, but struct
- type : not only function ptr, but any constant


## Read the test case
11. should be ok :
24. malloc `*mf_ptr` should handle specially(maybe never tested)


## TODO



## DEBUG FIX
1. bitcast point to self ?
2. 
  

**NO RA, unless totally sure !**



## Final question
#### `x = &y`

1. why is store instruction ?
There are some modication for the IR.

2. the influence for the store inst.

```c
#include <stdlib.h>
int plus(int a, int b) { return a+b; }

int minus(int a, int b) { return a-b; }
typedef  int (*GG)(int, int);
int moo(char x, int op1, int op2) {
    GG s = minus;
    GG s2 = plus;
    GG * gg = &s;
    GG * gg2 = &s2;
    GG ** ggg;
    if(x == 'a'){
      ggg = &gg;
    }else{
      ggg = &gg2;
    }
    (**ggg)(1,3);
    
    return 0;
}

// 41 : foo, clever
// 42 : plus, minus
```

```
; Function Attrs: noinline nounwind sspstrong uwtable
define dso_local i32 @moo(i8 signext, i32, i32) #0 !dbg !56 {
  %4 = alloca i32 (i32, i32)*, align 8
  %5 = alloca i32 (i32, i32)*, align 8
  %6 = alloca i32 (i32, i32)**, align 8
  %7 = alloca i32 (i32, i32)**, align 8
  call void @llvm.dbg.value(metadata i8 %0, metadata !60, metadata !DIExpression()), !dbg !61
  call void @llvm.dbg.value(metadata i32 %1, metadata !62, metadata !DIExpression()), !dbg !61
  call void @llvm.dbg.value(metadata i32 %2, metadata !63, metadata !DIExpression()), !dbg !61
  call void @llvm.dbg.declare(metadata i32 (i32, i32)** %4, metadata !64, metadata !DIExpression()), !dbg !66
  store i32 (i32, i32)* @minus, i32 (i32, i32)** %4, align 8, !dbg !66
  call void @llvm.dbg.declare(metadata i32 (i32, i32)** %5, metadata !67, metadata !DIExpression()), !dbg !68
  store i32 (i32, i32)* @plus, i32 (i32, i32)** %5, align 8, !dbg !68
  call void @llvm.dbg.declare(metadata i32 (i32, i32)*** %6, metadata !69, metadata !DIExpression()), !dbg !71
  store i32 (i32, i32)** %4, i32 (i32, i32)*** %6, align 8, !dbg !71
  call void @llvm.dbg.declare(metadata i32 (i32, i32)*** %7, metadata !72, metadata !DIExpression()), !dbg !73
  store i32 (i32, i32)** %5, i32 (i32, i32)*** %7, align 8, !dbg !73
  %8 = sext i8 %0 to i32, !dbg !74
  %9 = icmp eq i32 %8, 97, !dbg !76
  br i1 %9, label %10, label %11, !dbg !77

10:                                               ; preds = %3
  call void @llvm.dbg.value(metadata i32 (i32, i32)*** %6, metadata !78, metadata !DIExpression()), !dbg !61
  br label %12, !dbg !80

11:                                               ; preds = %3
  call void @llvm.dbg.value(metadata i32 (i32, i32)*** %7, metadata !78, metadata !DIExpression()), !dbg !61
  br label %12

12:                                               ; preds = %11, %10
  %.0 = phi i32 (i32, i32)*** [ %6, %10 ], [ %7, %11 ], !dbg !82
  call void @llvm.dbg.value(metadata i32 (i32, i32)*** %.0, metadata !78, metadata !DIExpression()), !dbg !61
  %13 = load i32 (i32, i32)**, i32 (i32, i32)*** %.0, align 8, !dbg !83
  %14 = load i32 (i32, i32)*, i32 (i32, i32)** %13, align 8, !dbg !84
  %15 = call i32 %14(i32 1, i32 3), !dbg !85
  ret i32 0, !dbg !86
}
```


## Meaning of store

```c
void K(GG a, GG b, int x){
  GG * s;
  if(x){
    s = &b;
  }else{
   s = &a;
  }
  if(x){
    *s = minus;
  }
  a(1, 2);
}
```


```
; Function Attrs: noinline nounwind sspstrong uwtable
define dso_local void @K(i32 (i32, i32)*, i32 (i32, i32)*, i32) #0 !dbg !56 {
  %4 = alloca i32 (i32, i32)*, align 8
  %5 = alloca i32 (i32, i32)*, align 8
  store i32 (i32, i32)* %0, i32 (i32, i32)** %4, align 8
  call void @llvm.dbg.declare(metadata i32 (i32, i32)** %4, metadata !60, metadata !DIExpression()), !dbg !61
  store i32 (i32, i32)* %1, i32 (i32, i32)** %5, align 8
  call void @llvm.dbg.declare(metadata i32 (i32, i32)** %5, metadata !62, metadata !DIExpression()), !dbg !63
  call void @llvm.dbg.value(metadata i32 %2, metadata !64, metadata !DIExpression()), !dbg !65
  %6 = icmp ne i32 %2, 0, !dbg !66
  br i1 %6, label %7, label %8, !dbg !68

7:                                                ; preds = %3
  call void @llvm.dbg.value(metadata i32 (i32, i32)** %5, metadata !69, metadata !DIExpression()), !dbg !65
  br label %9, !dbg !71

8:                                                ; preds = %3
  call void @llvm.dbg.value(metadata i32 (i32, i32)** %4, metadata !69, metadata !DIExpression()), !dbg !65
  br label %9

9:                                                ; preds = %8, %7
  %.0 = phi i32 (i32, i32)** [ %5, %7 ], [ %4, %8 ], !dbg !73
  call void @llvm.dbg.value(metadata i32 (i32, i32)** %.0, metadata !69, metadata !DIExpression()), !dbg !65
  %10 = icmp ne i32 %2, 0, !dbg !74
  br i1 %10, label %11, label %12, !dbg !76

11:                                               ; preds = %9
  store i32 (i32, i32)* @minus, i32 (i32, i32)** %.0, align 8, !dbg !77
  br label %12, !dbg !79

12:                                               ; preds = %11, %9
  %13 = load i32 (i32, i32)*, i32 (i32, i32)** %4, align 8, !dbg !80
  %14 = call i32 %13(i32 1, i32 2), !dbg !80
  ret void, !dbg !81
}
```

```c
int moo(char x, int op1, int op2) {

  struct fptr a;
  a.p_fptr = minus;
  a.p_fptr(1, 2);

  struct sfptr g;
  g.a = &a;
  g.a->p_fptr(1, 2);
  return 0;
}
```


```
; Function Attrs: noinline nounwind sspstrong uwtable
define dso_local i32 @moo(i8 signext, i32, i32) #0 !dbg !82 {
  %4 = alloca %struct.fptr, align 8
  %5 = alloca %struct.sfptr, align 8
  call void @llvm.dbg.value(metadata i8 %0, metadata !86, metadata !DIExpression()), !dbg !87
  call void @llvm.dbg.value(metadata i32 %1, metadata !88, metadata !DIExpression()), !dbg !87
  call void @llvm.dbg.value(metadata i32 %2, metadata !89, metadata !DIExpression()), !dbg !87
  call void @llvm.dbg.declare(metadata %struct.fptr* %4, metadata !90, metadata !DIExpression()), !dbg !91
  %6 = getelementptr inbounds %struct.fptr, %struct.fptr* %4, i32 0, i32 0, !dbg !92
  store i32 (i32, i32)* @minus, i32 (i32, i32)** %6, align 8, !dbg !93
  %7 = getelementptr inbounds %struct.fptr, %struct.fptr* %4, i32 0, i32 0, !dbg !94
  %8 = load i32 (i32, i32)*, i32 (i32, i32)** %7, align 8, !dbg !94
  %9 = call i32 %8(i32 1, i32 2), !dbg !95
  call void @llvm.dbg.declare(metadata %struct.sfptr* %5, metadata !96, metadata !DIExpression()), !dbg !100
  %10 = getelementptr inbounds %struct.sfptr, %struct.sfptr* %5, i32 0, i32 0, !dbg !101
  store %struct.fptr* %4, %struct.fptr** %10, align 8, !dbg !102
  %11 = getelementptr inbounds %struct.sfptr, %struct.sfptr* %5, i32 0, i32 0, !dbg !103
  %12 = load %struct.fptr*, %struct.fptr** %11, align 8, !dbg !103
  %13 = getelementptr inbounds %struct.fptr, %struct.fptr* %12, i32 0, i32 0, !dbg !104
  %14 = load i32 (i32, i32)*, i32 (i32, i32)** %13, align 8, !dbg !104
  %15 = call i32 %14(i32 1, i32 2), !dbg !105
  ret i32 0, !dbg !106
}
```

