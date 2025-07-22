#include "llvm/CodeGen/GlobalISel/MachineIRBuilder.h"
#include "llvm/CodeGen/MachineFrameInfo.h" // For CreateStackObject.
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineMemOperand.h" // For MachinePointerInfo.
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/Register.h"
#include "llvm/CodeGen/TargetOpcodes.h"     // For INLINEASM.
#include "llvm/CodeGenTypes/LowLevelType.h" // For LLT.
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h" // For ICMP_EQ.

using namespace llvm;
/*
Takes p Foo and applies a simple constant propagation optimization.
returns true if p Foo was modified (i.e., something had been constant
propagated), false otherwise.

Goal: 
Simplify computations by replacing variables with constants and 
combining the constants to produce fewer computations.

Assumptions:
- Only int types are constant propagated
- Constant propagation is always legal and profitable
- We give up on constants when the constant type changes

APIs:
- ConstantInt (subclass of Constant class in the IR lib) will hold the values of constants to propagate for optimization
  - ConstantInt::get(LLVMContext &Context, const APInt &V): create a new ConstantInt instance from the result of your APInt calculation. 
  - dyn_cast<ConstantInt>(Value): Use this to check if a Value is an integer constant and to get a ConstantInt object from it.
  - const APInt &ConstantInt::getValue(): generic way to get the const value as an APInt object.
- APInt class (from the ADT library): represents arbitrary precision integers, used to hold the constant values.
- Mod the IR with the new value.
  - Value::replaceAllUsesWith(Value *NewVal): Call this on the original instruction to replace all of its uses throughout the function with your new constant value 
*/

bool myConstantPropagation(Function &Foo) {
  // Traverse the instructions in a given LLVM Function.
  // Identify instructions that perform arithmetic on constant integer values.
  // Compute the result of these operations at compile time.
  // Replace the original instruction with the new, simplified constant result.
  // Return true if you changed the code, and false otherwise.

  // Keep track of whether we made any changes.
  bool changed = false;
  
  // List or vector to keep track of the instructions to be removed.
  std::vector<Instruction *> ToRemove;

  // LLVM Function is a collection of basic blocks.
  // Each BB is a sequence of Instructions.
  // Visit every instruction to see if you can optimize it.
  for (BasicBlock &BB : Foo) { 
    for (Instruction &Inst : BB) {
      // Check if the instruction can fold
      // Only consider simple integer arithmetic
      // Check if the instruction is a BinaryOperator
      if (llvm::isa<llvm::BinaryOperator>(Inst)) {
        // Check if both operands are constants
        auto *LHS = dyn_cast<ConstantInt>(Inst.getOperand(0));
        auto *RHS = dyn_cast<ConstantInt>(Inst.getOperand(1));
        if (LHS && RHS) {
          // Fold the result
          APInt LHSValue = LHS->getValue();
          APInt RHSValue = RHS->getValue();
          APInt ResultValue;

          switch (Inst.getOpcode()) {
            case Instruction::Add:
              ResultValue = LHSValue + RHSValue;
              break;
            case Instruction::Sub:
              ResultValue = LHSValue - RHSValue;
              break;
            case Instruction::Mul:
              ResultValue = LHSValue * RHSValue;
              break;
            case Instruction::SDiv:
              ResultValue = LHSValue.sdiv(RHSValue);
              break;
            case Instruction::Shl:
              ResultValue = LHSValue.shl(RHSValue);
              break;
            case Instruction::Or:
              ResultValue = LHSValue | RHSValue;
              break;
            default:
              continue; // Skip unsupported operations
          }
          
          // Create new LLVMContext from result
          LLVMContext &Context = Foo.getContext();
          Constant *NewConstant = ConstantInt::get(Context, ResultValue);

          // Replace old instructions
          Inst.replaceAllUsesWith(NewConstant);

          // Handle deletion with a worklist
          ToRemove.push_back(&Inst);

          changed = true;
        }
      }
    }
  }

  for (Instruction *I : ToRemove) {
    I->eraseFromParent();
  }

  return changed;
}