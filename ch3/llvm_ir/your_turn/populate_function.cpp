#include "llvm/ADT/ArrayRef.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"    // For ConstantInt.
#include "llvm/IR/DerivedTypes.h" // For PointerType, FunctionType.
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
// #include "llvm/Support/Debug.h" // For errs().
#include "llvm/Support/raw_ostream.h" // For printing the IR.

#include <memory> // For unique_ptr

using namespace llvm;

// The goal of this function is to build a Module that
// represents the lowering of the following foo, a C function:
// extern int baz();
// extern void bar(int);
// void foo(int a, int b) {
//   int var = a + b;
//   if (var == 0xFF) {
//     bar(var);
//     var = baz();
//   }
//   bar(var);
// }
//
// The IR for this snippet (at O0) is:
// define void @foo(i32 %arg, i32 %arg1) {
// bb:
//   %i = alloca i32
//   %i2 = alloca i32
//   %i3 = alloca i32
//   store i32 %arg, ptr %i
//   store i32 %arg1, ptr %i2
//   %i4 = load i32, ptr %i
//   %i5 = load i32, ptr %i2
//   %i6 = add i32 %i4, %i5
//   store i32 %i6, ptr %i3
//   %i7 = load i32, ptr %i3
//   %i8 = icmp eq i32 %i7, 255
//   br i1 %i8, label %bb9, label %bb12
//
// bb9:
//   %i10 = load i32, ptr %i3
//   call void @bar(i32 %i10)
//   %i11 = call i32 @baz()
//   store i32 %i11, ptr %i3
//   br label %bb12
//
// bb12:
//   %i13 = load i32, ptr %i3
//   call void @bar(i32 %i13)
//   ret void
// }
//
// declare void @bar(i32)
// declare i32 @baz(...)

std::unique_ptr<Module> myBuildModule(LLVMContext &Ctxt) { 
    // Create the high level module.
    // This is the module that will contain all the functions.
    // std::unique_ptr<Module> M 
    auto M = std::make_unique<Module>("MyFirstModule", Ctxt);

    // Types and Function Types
    // Why no *PtrTy
    // Int23
    Type *Int32Ty = Type::getInt32Ty(Ctxt);
    // Void
    Type *VoidTy = Type::getVoidTy(Ctxt);
    // int baz();
    FunctionType *BazFuncTy = FunctionType::get(Int32Ty, /*isVarArg=*/false);
    // void bar(int);
    FunctionType *BarFuncTy = FunctionType::get(VoidTy, {Int32Ty}, false);
    // void foo(int a, int b) {
    FunctionType *FooFuncTy = FunctionType::get(VoidTy, {Int32Ty, Int32Ty}, false);

    // Define the external functions for bax, bar, and foo.
    FunctionCallee BazFunc = M->getOrInsertFunction("baz", BazFuncTy);
    // getOrInsertFunction returns a FunctionCallee
    // FunctionCallee is a wrapper class that holds anything "callable," usually a function ptr (Function*)
    FunctionCallee BarFunc = M->getOrInsertFunction("bar", BarFuncTy);
    // Need to build the foo function. Using Function::Create to create a new function in the module.
    Function *FooFunc = Function::Create(FooFuncTy, Function::ExternalLinkage, "foo", M.get());

    // Improve the IR readability 
    // %0 to %a, and %1 to %b
    FooFunc->getArg(0)->setName("a");
    FooFunc->getArg(1)->setName("b");

    // Create the basic blocks for the foo function.
    // Need 'Entry', 'Then', and 'Merge' blocks.
    BasicBlock *EntryBB = BasicBlock::Create(Ctxt, "entry", FooFunc); // Where the function starts.
    BasicBlock *ThenBB = BasicBlock::Create(Ctxt, "then_block", FooFunc); // For the if body.
    BasicBlock *MergeBB = BasicBlock::Create(Ctxt, "merge_block", FooFunc); // Where control flow merges after the if.

    // User the IRbuilder to populate the basic blocks.
    // IRBuilder is a helper class to create and insert instructions into basic blocks.
    IRBuilder<> Builder(EntryBB);
    // Allocate stack space for "var"
    AllocaInst *VarAlloca = Builder.CreateAlloca(Int32Ty, nullptr, "var.addr");
    // Get teh args 'a' and 'b' from the function.
    Value *ArgA = FooFunc->getArg(0);
    Value *ArgB = FooFunc->getArg(1);
    // int var = a + b;
    // 1. Create the add instruction.
    Value *Sum = Builder.CreateAdd(ArgA, ArgB, "sum");
    // 2. Store the result into the stack space allocated for 'var'.
    Builder.CreateStore(Sum, VarAlloca);

    // if (var == 0xFF)
    // 1. Load the value of 'var' from the stack.
    Value *VarVal = Builder.CreateLoad(Int32Ty, VarAlloca, "var");
    // 2. Create the constant value for 255 (0xFF).
    Value *Cst255 = ConstantInt::get(Int32Ty, 255);
    // 3. Create the comparison instruction.
    Value *Cmp = Builder.CreateICmpEQ(VarVal, Cst255, "cmp");
    // 4. Create the conditional branch to 'Then' or 'Merge'.
    Builder.CreateCondBr(Cmp, ThenBB, MergeBB);

    // Populate the 'Then' block.
    Builder.SetInsertPoint(ThenBB);

    // Load the value of 'var' again.
    Value *VarForBar = Builder.CreateLoad(Int32Ty, VarAlloca, "var.for.bar");
    // Create the call bar(var).
    Builder.CreateCall(BarFunc, VarForBar);

    // 'var = baz()'
    // Call baz() and get its int result.
    Value *BazResult = Builder.CreateCall(BazFunc, {}, "baz.result");
    // Store the result back into 'var'.
    Builder.CreateStore(BazResult, VarAlloca);

    // After the 'Then' block, we need to unconditionally jump to the merge block.
    Builder.CreateBr(MergeBB);

    // Populate the 'Merge' block.
    Builder.SetInsertPoint(MergeBB);

    // 'bar(var)'
    // Load the value of 'var' again.
    Value *FinalVar = Builder.CreateLoad(Int32Ty, VarAlloca, "var.final");
    // Create the call to bar
    Builder.CreateCall(BarFunc, FinalVar);

    // Create the final return.
    // Remember all basic blocks must end with a terminator.
    Builder.CreateRetVoid();

    // Return the module.
    M->print(errs(), nullptr); // Print the module to stderr for debugging.    

    return M; 
}