#include "Core/CPU/CPUCore.h"
#include "Core/CPU/IR.h"
#include "Core/CPU/IntrusiveIRList.h"
#include "LLVM.h"
#include "LogManager.h"
#include <map>
#include <llvm/InitializePasses.h>
#include <llvm/LinkAllPasses.h>
#include <llvm/PassRegistry.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/IRPrintingPasses.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm-c/Core.h>

namespace Emu {
class LLVM final : public CPUBackend {
public:
	LLVM();
	~LLVM();
  std::string GetName() override { return "LLVM"; }
  void* CompileCode(Emu::IR::IntrusiveIRList const *ir) override;
private:
	LLVMContextRef con;
	llvm::Module *mainmodule;
	llvm::ExecutionEngine *engine;
};

LLVM::LLVM() {
	using namespace llvm;
	InitializeNativeTarget();
	InitializeNativeTargetAsmPrinter();
	con = LLVMContextCreate();
	mainmodule = new llvm::Module("Main Module", **llvm::unwrap(&con));
	engine = EngineBuilder(std::unique_ptr<llvm::Module>(mainmodule))
		.setEngineKind(EngineKind::JIT)
		.create();
}

LLVM::~LLVM() {
	delete engine;
	LLVMContextDispose(con);
}

void* LLVM::CompileCode(Emu::IR::IntrusiveIRList const *ir) {
  return nullptr;
};

CPUBackend *CreateLLVMBackend() {
  return new LLVM();
}

}
