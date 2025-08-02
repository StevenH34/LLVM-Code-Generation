#include "llvm/IR/Function.h"
#include "llvm/Pass.h"          // For FunctionPass & INITIALIZE_PASS.
#include "llvm/Support/Debug.h" // For errs().

using namespace llvm;

extern bool solutionConstantPropagation(llvm::Function &);

// The implementation of this function is generated at the end of this file. See
// INITIALIZE_PASS.
namespace llvm {
void initializeYourTurnConstantPropagationPass(PassRegistry &);
};

namespace {
// Inherit from llvm::FunctionPass 
class YourTurnConstantPropagation : public FunctionPass {
public:
  static char ID; // Each legacy pass needs an ID
  // The constructor needs to call FunctionPass() with the ID
  YourTurnConstantPropagation() : FunctionPass(ID) {}
  // Override the virtual function from the base class
  bool runOnFunction(llvm::Function &F) override {
    // runOnFunction must return a bool
    // Return true if the function F was modified
    // Call the optimization, which returns a bool
    return solutionConstantPropagation(F);
  }
};
} // End anonymous namespace.

// Need to define ID so it can allocate memory for the ID 
// so its address can be used as a unique identifier.
char YourTurnConstantPropagation::ID = 0;

Pass *createYourTurnPassForLegacyPM() {
  // Return a new instance of YourTurnConstantPropagation class
  return new YourTurnConstantPropagation();
}

// Add the INITIALIZE_PASS macro
// INITIALIZE_PASS(passName, argString, name, cfg, analysis)
// passName is the class name
// argString is the cmd line string used to invoke your pass
// name is the description of your pass
// cfg is a Boolean that tells the pass manager whether your pass only looks at the CFG
// analysis is a Boolean that tells whether your pass is an analysis
INITIALIZE_PASS(YourTurnConstantPropagation, "your-turn-const-prop", "Simple constant propagation", false, false);

// Cannot define a function belonging to namespace llvm from within an anonymous namespace.