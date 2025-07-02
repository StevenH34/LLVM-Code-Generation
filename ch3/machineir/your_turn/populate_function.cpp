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

// The goal of this function is to build a MachineFunction that
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
// The proposed ABI is:
// - 32-bit arguments are passed through registers: w0, w1
// - 32-bit returned values are passed through registers: w0, w1
// w0 and w1 are given as argument of this Function.
//
// The local variable named var is expected to live on the stack.
MachineFunction *populateMachineIR(MachineModuleInfo &MMI, Function &Foo, Register W0, Register W1) {
  // MachineFunction obj that represents the function Foo.
  MachineFunction &MF = MMI.getOrCreateMachineFunction(Foo);
  // Manages all register information for the function and stores it into a local var. 
  // getRegInfo() is a method of the MachineFunction class. It returns a ref to the MachineRegisterInfo obj.
  // MachineRegisterInfo is a data structure in the LLVM backend. It keeps track of all used registers.
  //    Virtual registers: temp registers that act as placeholders for values
  //    Physical registers: hardware registers that target architecture 
  // &MRI is a ref pointing to the MachineRegisterInfo obj.
  MachineRegisterInfo &MRI = MF.getRegInfo();

  // The type for bool.
  LLT I1 = LLT::scalar(1);
  // The type of var.
  LLT I32 = LLT::scalar(32);

  // The stack slot for var.
  Align VarStackAlign(4);
  int FrameIndex = MF.getFrameInfo().CreateStackObject(I32.getSizeInBytes(), VarStackAlign, /*IsSpillSlot=*/false);

  // To use to create load and store for var.
  MachinePointerInfo PtrInfo = MachinePointerInfo::getFixedStack(MF, FrameIndex);

  // The type for the address of var.
  LLT VarAddrLLT = LLT::pointer(/*AddressSpace=*/0, /*SizeInBits=*/64);

  // Create the 3 basic blocks that compose Foo.
  // Entry basic block is the entry point of the function.
  // Then basic block for the if condition
  // Exit basic block for the end of the function.
  MachineBasicBlock *EntryBB = MF.CreateMachineBasicBlock();
  MachineBasicBlock *ThenBB = MF.CreateMachineBasicBlock();
  MachineBasicBlock *ExitBB = MF.CreateMachineBasicBlock();

  // Insert the basic blocks into the function.
  MF.push_back(EntryBB);
  MF.push_back(ThenBB);
  MF.push_back(ExitBB);

  // Define the CFG of the function.
  EntryBB->addSuccessor(ThenBB);
  EntryBB->addSuccessor(ExitBB);
  ThenBB->addSuccessor(ExitBB);

  // POPULATE ENTRY BASIC BLOCK
  {
    MachineIRBuilder MIBuilder(*EntryBB, EntryBB->end());
    // Get the input arguments.
    Register RegA = MIBuilder.buildCopy(I32, W0).getReg(0);
    Register RegB = MIBuilder.buildCopy(I32, W1).getReg(0);

    // Register add
    Register RegAdd = MIBuilder.buildAdd(I32, RegA, RegB).getReg(0);

    // Store res (a + b) in stack slot 'var'. 
    // Gen the mem address of the stack slot (for 'var').
    Register VarStackAddr = MIBuilder.buildFrameIndex(VarAddrLLT, FrameIndex).getReg(0);
    // Creates a metadata object that describes the memory operation.
    MachineMemOperand *MMOStore = MF.getMachineMemOperand(
          PtrInfo, MachineMemOperand::MOStore, I32.getSizeInBytes(), VarStackAlign);
    // Builds the store instruction.
    // Writes the val of RegAdd to the stack slot address VarStackAddr.
    MIBuilder.buildStore(RegAdd, VarStackAddr, *MMOStore);

    // if (var == 0xFF)
    // Create a virtual register for the constant.
    Register Const0xFF = MIBuilder.buildConstant(I32, 0xFF).getReg(0);
    // Compare 'var' with 0xFF.
    Register Cond = MIBuilder.buildICmp(CmpInst::ICMP_EQ, I1, RegAdd, Const0xFF).getReg(0);

    // Conditional branch.
    MIBuilder.buildBrCond(Cond, *ThenBB);
    // Unconditional branch
    MIBuilder.buildBr(*ExitBB);
  }

  // POPULATE THEN BASIC BLOCK
  {
    MachineIRBuilder MIBuilder(*ThenBB, ThenBB->end());
    // Load 'var'
    Register VarStackAddr = MIBuilder.buildFrameIndex(VarAddrLLT, FrameIndex).getReg(0);
    MachineMemOperand *MMOLoad1 = MF.getMachineMemOperand(
          PtrInfo, MachineMemOperand::MOLoad, I32.getSizeInBytes(), VarStackAlign);
    Register RegVar1 = MIBuilder.buildLoad(I32, VarStackAddr, *MMOLoad1).getReg(0);

    // Copy 'var'
    MIBuilder.buildCopy(W0, RegVar1);
    // Call bar(var)
    MIBuilder.buildInstr(TargetOpcode::INLINEASM, {}, {})
      .addExternalSymbol("bl @bar")
      .addImm(0)
      .addReg(W0, RegState::Implicit);

    // var = baz();
    MIBuilder.buildInstr(TargetOpcode::INLINEASM, {}, {})
      .addExternalSymbol("bl @baz")
      .addImm(0)
      .addReg(W0, RegState::Implicit | RegState::Define);

    // Store the result of baz
    Register ResOfBaz = MIBuilder.buildCopy(I32, W0).getReg(0);

    // Store the result of baz in 'var'.
    MachineMemOperand *MMOStore = MF.getMachineMemOperand(PtrInfo, MachineMemOperand::MOStore, I32.getSizeInBytes(), VarStackAlign);
    MIBuilder.buildStore(ResOfBaz, VarStackAddr, PtrInfo, VarStackAlign);

    MIBuilder.buildBr(*ExitBB);
  }

  // EXIT BASIC BLOCK
  {
    MachineIRBuilder MIBuilder(*ExitBB, ExitBB->end());
    MIBuilder.setInsertPt(*ExitBB, ExitBB->end());

    // Load 'var' again.
    Register VarStackAddr = MIBuilder.buildFrameIndex(VarAddrLLT, FrameIndex).getReg(0);
    MachineMemOperand *MMOLoad = MF.getMachineMemOperand(PtrInfo, MachineMemOperand::MOLoad, I32.getSizeInBytes(), VarStackAlign);
    Register RegVar2 = MIBuilder.buildLoad(I32, VarStackAddr, *MMOLoad).getReg(0);

    // Copy 'var'
    MIBuilder.buildCopy(W0, RegVar2);
    MIBuilder.buildInstr(TargetOpcode::INLINEASM, {}, {})
      .addExternalSymbol("bl @bar")
      .addImm(0)
      .addReg(W0, RegState::Implicit);

    // 'return'
    MIBuilder.buildInstr(TargetOpcode::INLINEASM, {}, {})
      .addExternalSymbol("ret")
      .addImm(0);
  }

  return &MF;
}
