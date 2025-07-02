#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/raw_ostream.h"

#include <memory>

using namespace llvm;

// Forward-declare the functions from the other .cpp files
extern std::unique_ptr<Module> myBuildModule(LLVMContext &);
extern std::unique_ptr<Module> solutionBuildModule(LLVMContext &);

int main(int argc, char **argv) {
  LLVMContext Ctxt;
  bool hadError = false;

  for (int i = 0; i != 2; ++i) {
    bool isRefImpl = i == 0;
    std::unique_ptr<Module> CurModule =
        isRefImpl ? solutionBuildModule(Ctxt) : myBuildModule(Ctxt);
    const char *msg = isRefImpl ? "Reference" : "Your solution";

    outs() << "\n\n## Processing module from " << msg << " implementation\n";
    if (!CurModule) {
      outs() << "Nothing built\n";
      continue;
    }

    CurModule->print(errs(), /*AssemblyAnnotationWriter=*/nullptr);

    // verifyModule returns true if it finds errors
    if (verifyModule(*CurModule, &errs())) {
      errs() << msg << " module does not verify\n";
      hadError |= true;
    } else {
      errs() << msg << " module verified.\n";
    }
  }

  return hadError; // Return 0 on success, 1 on error
}