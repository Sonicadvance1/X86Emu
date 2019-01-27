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

using namespace llvm;

namespace Emu {
class LLVM final : public CPUBackend {
public:
	LLVM(Emu::CPUCore* CPU);
	~LLVM();
  std::string GetName() override { return "LLVM"; }
  void* CompileCode(Emu::IR::IntrusiveIRList const *ir) override;

private:
  void CreateGlobalVariables(llvm::ExecutionEngine *engine, llvm::Module *module);
  llvm::Value *CreateContextGEP(uint64_t Offset);
  void HandleIR(uint64_t Offset, IR::IROp_Header const* op);
  std::map<uint64_t, llvm::Value*> Values;
  llvm::LLVMContext *con;
  LLVMContextRef conref;
	llvm::Module *mainmodule;
  llvm::IRBuilder<> *builder;
  std::vector<llvm::ExecutionEngine*> functions;
  Emu::CPUCore *cpu;

  struct GlobalState {
    // CPUState
    llvm::Type *cpustatetype;
    GlobalVariable *cpustatevar;
    llvm::LoadInst *cpustate;
    llvm::Function *syscallfunction;
    llvm::Function *loadmem4function;
    llvm::Function *loadmem8function;
  };
  GlobalState state;
  llvm::Function *func;
  std::vector<BasicBlock*> BlockStack;
  std::unordered_map<uint64_t, bool> JumpTargets;
  std::unordered_map<uint64_t, BasicBlock*> BlockJumpTargets;

  uint64_t CurrentRIP{0};
  void FindJumpTargets(Emu::IR::IntrusiveIRList const *ir);
};

LLVM::LLVM(Emu::CPUCore *CPU)
  : cpu {CPU} {
	using namespace llvm;
	InitializeNativeTarget();
	InitializeNativeTargetAsmPrinter();
	conref = LLVMContextCreate();
  con = *llvm::unwrap(&conref);
	mainmodule = new llvm::Module("Main Module", *con);
  builder = new IRBuilder<>(*con);
}

LLVM::~LLVM() {
  delete builder;
  for (auto module : functions)
    delete module;
	LLVMContextDispose(conref);
}

static uint64_t LoadMem4(CPUCore *cpu, uint64_t Offset) {
  return *cpu->MemoryMapper->GetBaseOffset<uint32_t*>(Offset);
}

static uint64_t LoadMem8(CPUCore *cpu, uint64_t Offset) {
  return *cpu->MemoryMapper->GetBaseOffset<uint64_t*>(Offset);
}

void LLVM::CreateGlobalVariables(llvm::ExecutionEngine *engine, llvm::Module *module) {
  // CPUState types
  Type *i64 = Type::getInt64Ty(*con);


  //uint64_t rip;
  //uint64_t gregs[16];
  //uint64_t xmm[16][2];
  //uint64_t gs;
  //uint64_t fs;
  state.cpustatetype = StructType::create(*con,
      {
        i64, // RIP
        ArrayType::get(i64, 16),
        ArrayType::get(ArrayType::get(i64, 2), 16),
        i64,
        i64,
        i64,
      });
  module->getOrInsertGlobal("X86StateGlobal", state.cpustatetype->getPointerTo());
  state.cpustatevar = module->getNamedGlobal("X86StateGlobal");
  state.cpustatevar->setInitializer(ConstantInt::getIntegerValue(state.cpustatetype->getPointerTo(), APInt(64, (uint64_t)&cpu->ParentThread->CPUState)));
  state.cpustate = builder->CreateLoad(state.cpustatevar, builder->getInt32(0), "X86State");

  {
//  struct SyscallArguments {
//    static constexpr std::size_t MAX_ARGS = 7;
//    uint64_t Argument[MAX_ARGS];
//  };

    auto functype = FunctionType::get(i64,
        {
          i64,
          ArrayType::get(i64, SyscallHandler::SyscallArguments::MAX_ARGS)->getPointerTo(),
        },
        false);
    state.syscallfunction = Function::Create(functype,
        Function::ExternalLinkage,
        "Syscall",
        module);

    using ClassPtr = uint64_t (Emu::SyscallHandler::*)(SyscallHandler::SyscallArguments *Args);
    union Test{
      ClassPtr ClassData;
      void* Data;
    };
    Test A;
    A.ClassData = &Emu::SyscallHandler::HandleSyscall;
    engine->addGlobalMapping(state.syscallfunction, A.Data);

  auto loadmemfunctype = FunctionType::get(i64,
        {
          i64,
          i64,
        },
        false);

    state.loadmem4function = Function::Create(loadmemfunctype,
        Function::ExternalLinkage,
        "LoadMem4",
        module);
    engine->addGlobalMapping(state.loadmem4function, (void*)LoadMem4);

    state.loadmem8function = Function::Create(loadmemfunctype,
        Function::ExternalLinkage,
        "LoadMem8",
        module);
    engine->addGlobalMapping(state.loadmem8function, (void*)LoadMem8);
  }
}

llvm::Value *LLVM::CreateContextGEP(uint64_t Offset) {
  std::vector<llvm::Value*> gepvalues = {
    builder->getInt32(0), // "first" instance of cpustate
  };
  if (Offset == 0) { // RIP
    gepvalues.emplace_back(builder->getInt32(0));
  }
  else if (Offset >= offsetof(X86State, gregs) && Offset < offsetof(X86State, xmm)) {
    gepvalues.emplace_back(builder->getInt32(1));
    gepvalues.emplace_back(builder->getInt32((Offset - offsetof(X86State, gregs)) / 8));
  }
  else if (Offset == offsetof(X86State, gs)) {
    gepvalues.emplace_back(builder->getInt32(3));
  }
  else if (Offset == offsetof(X86State, fs)) {
    gepvalues.emplace_back(builder->getInt32(4));
  }
  else if (Offset == offsetof(X86State, rflags)) {
    gepvalues.emplace_back(builder->getInt32(5));
  }
  else
    std::abort();

  auto gep = builder->CreateGEP(state.cpustate, gepvalues, "Context");
  return gep;
}

void LLVM::HandleIR(uint64_t Offset, IR::IROp_Header const* op) {
//  printf("IR Op %zd: %d(%s)\n", Offset, op->Op, Emu::IR::GetName(op->Op).c_str());

  switch (op->Op) {
  case IR::OP_BEGINBLOCK:
  break;
  case IR::OP_RIP_MARKER: {
    auto Op = op->C<IR::IROp_RIPMarker>();
    CurrentRIP = Op->RIP;
    if (JumpTargets.find(CurrentRIP) != JumpTargets.end()) {
      // We are a jump target!
      // Create a new block
      auto NewBlock = BasicBlock::Create(*con, "new", func);
      // Make our current block branch in to the new one
      builder->CreateBr(NewBlock);

      builder->SetInsertPoint(NewBlock);
      BlockJumpTargets[CurrentRIP] = NewBlock;
      printf("\tAdding Block Target: %zx\n", CurrentRIP);
    }
  break;
  }
  case IR::OP_ENDBLOCK: {
    auto EndOp = op->C<IR::IROp_EndBlock>();
    auto downcountValue = builder->CreateGEP(state.cpustate,
        {
          builder->getInt32(0),
          builder->getInt32(0),
        }, "RIP");
    auto load = builder->CreateLoad(downcountValue);
    auto newvalue = builder->CreateAdd(load, builder->getInt64(EndOp->RIPIncrement));
    builder->CreateStore(newvalue, downcountValue);
    builder->CreateRetVoid();

    // If we are at the end of a block and we have blocks in our stack then change over to that as an active block
    if (BlockStack.size()) {
      builder->SetInsertPoint(BlockStack.back());
      BlockStack.pop_back();
    }
  break;
  }
  case IR::OP_JUMP_TGT:
    printf("Landed on jump target\n");
  break;
  case IR::OP_COND_JUMP: {
    // Conditional jump
    // if the value is true then it'll jump to the target
    // if the value is false then it'll fall through
    auto JumpOp = op->C<IR::IROp_CondJump>();

    auto TruePath = BasicBlock::Create(*con, "true", func);
    auto FalsePath = BasicBlock::Create(*con, "false", func);

    auto Comp = builder->CreateICmpNE(Values[JumpOp->Cond], builder->getInt64(0));

    printf("\tJUMP wanting to go to RIP: 0x%zx\n", JumpOp->RIPTarget);
    auto target = BlockJumpTargets.find(JumpOp->RIPTarget);
    if (target != BlockJumpTargets.end()) {
      printf("\tCOND JUMP Has a found rip target!\n");
      builder->CreateCondBr(Comp, target->second, FalsePath);
      builder->SetInsertPoint(TruePath);
    } else {

      builder->CreateCondBr(Comp, TruePath, FalsePath);

      builder->SetInsertPoint(TruePath);
    }

    // Will create a dead block for us in the case we have a real target
    BlockStack.emplace_back(FalsePath);

  }
  break;
  case IR::OP_CONSTANT: {
    auto ConstantOp = op->C<IR::IROp_Constant>();
    Values[Offset] = builder->getInt64(ConstantOp->Constant);
  }
  break;
  case IR::OP_SYSCALL: {
    auto SyscallOp = op->C<IR::IROp_Syscall>();

    std::vector<Value*> Args;
    Args.emplace_back(builder->getInt64((uint64_t)&cpu->syscallhandler));

    auto args = builder->CreateAlloca(ArrayType::get(Type::getInt64Ty(*con), SyscallHandler::SyscallArguments::MAX_ARGS));
    for (int i = 0; i < IR::IROp_Syscall::MAX_ARGS; ++i) {
      auto location = builder->CreateGEP(args,
          {
            builder->getInt32(0),
            builder->getInt32(i),
          },
          "Arg");
      builder->CreateStore(Values[SyscallOp->Arguments[i]], location);
    }
    Args.emplace_back(args);

    Values[Offset] = builder->CreateCall(state.syscallfunction, Args);
  break;
  }

  case IR::OP_LOADCONTEXT: {
    auto LoadOp = op->C<IR::IROp_LoadContext>();
    LogMan::Throw::A(LoadOp->Size == 8, "Can only handle 8 byte");

    auto Value = CreateContextGEP(LoadOp->Offset);
    auto load = builder->CreateLoad(Value);
    Values[Offset] = load;
  }
  break;
  case IR::OP_STORECONTEXT: {
    auto StoreOp = op->C<IR::IROp_StoreContext>();
    LogMan::Throw::A(StoreOp->Size == 8, "Can only handle 8 byte");

    auto Value = CreateContextGEP(StoreOp->Offset);
    builder->CreateStore(Values[StoreOp->Arg], Value);
  }
  break;
  case IR::OP_ADD: {
    auto AddOp = op->C<IR::IROp_Add>();
    Values[Offset] = builder->CreateAdd(Values[AddOp->Args[0]], Values[AddOp->Args[1]]);
  }
  break;
  case IR::OP_SUB: {
    auto SubOp = op->C<IR::IROp_Sub>();
    Values[Offset] = builder->CreateSub(Values[SubOp->Args[0]], Values[SubOp->Args[1]]);
  }
  break;
  case IR::OP_OR: {
    auto OrOp = op->C<IR::IROp_Or>();
    Values[Offset] = builder->CreateOr(Values[OrOp->Args[0]], Values[OrOp->Args[1]]);
  }
  break;
  case IR::OP_XOR: {
    auto XorOp = op->C<IR::IROp_Xor>();
    Values[Offset] = builder->CreateXor(Values[XorOp->Args[0]], Values[XorOp->Args[1]]);
  }
  break;
  case IR::OP_SHL: {
    auto ShlOp = op->C<IR::IROp_Shl>();
    Values[Offset] = builder->CreateShl(Values[ShlOp->Args[0]], Values[ShlOp->Args[1]]);
  }
  break;
  case IR::OP_SHR: {
    auto ShrOp = op->C<IR::IROp_Shr>();
    Values[Offset] = builder->CreateLShr(Values[ShrOp->Args[0]], Values[ShrOp->Args[1]]);
  }
  break;
  case IR::OP_AND: {
    auto AndOp = op->C<IR::IROp_And>();
    Values[Offset] = builder->CreateAnd(Values[AndOp->Args[0]], Values[AndOp->Args[1]]);
  }
  break;
  case IR::OP_NAND: {
    auto NandOp = op->C<IR::IROp_Nand>();
    Values[Offset] = builder->CreateAnd(Values[NandOp->Args[0]], builder->CreateNot(Values[NandOp->Args[1]]));
  }
  break;
  case IR::OP_SELECT: {
    auto SelectOp = op->C<IR::IROp_Select>();
    Value *Comp;
    switch (SelectOp->Op) {
    case IR::IROp_Select::COMP_EQ:
      Comp = builder->CreateICmpEQ(Values[SelectOp->Args[0]], Values[SelectOp->Args[1]]);
      break;
    case IR::IROp_Select::COMP_NEQ:
      Comp = builder->CreateICmpNE(Values[SelectOp->Args[0]], Values[SelectOp->Args[1]]);
      break;
    default:
      printf("Unknown comparison case\n");
      std::abort();
      break;
    };
    Values[Offset] = builder->CreateSelect(Comp, Values[SelectOp->Args[2]], Values[SelectOp->Args[3]]);
  }
  break;
  case IR::OP_BITEXTRACT: {
    auto BitExtractOp = op->C<IR::IROp_BitExtract>();
    auto SHR = builder->CreateLShr(Values[BitExtractOp->Args[0]], Values[BitExtractOp->Args[1]]);
    auto Mask = builder->CreateAnd(SHR, builder->getInt64(1));
    Values[Offset] = Mask;
  }
  break;
  case IR::OP_TRUNC_32: {
    auto Trunc_32Op = op->C<IR::IROp_Trunc_32>();
    auto Arg = builder->CreateTrunc(Values[Trunc_32Op->Arg], Type::getInt32Ty(*con));
    Arg = builder->CreateZExt(Arg, Type::getInt64Ty(*con));
    Values[Offset] = Arg;
  }
  break;
  case IR::OP_LOAD_MEM: {
    auto LoadMemOp = op->C<IR::IROp_LoadMem>();
    Value *Src = Values[LoadMemOp->Arg[0]];
    if (LoadMemOp->Arg[1] != ~0)
      Src = builder->CreateAdd(Src, Values[LoadMemOp->Arg[1]]);
#if 0
    std::vector<Value*> Args;
    Args.emplace_back(builder->getInt64((uint64_t)cpu));
    Args.emplace_back(Src);

    switch (LoadMemOp->Size) {
    case 4:
      Values[Offset] = builder->CreateCall(state.loadmem4function, Args);
    break;
    case 8:
      Values[Offset] = builder->CreateCall(state.loadmem8function, Args);
    break;
    default:
    printf("Unknown LoadSize: %d\n", LoadMemOp->Size);
    std::abort();
    break;
    }
#else
    Src = builder->CreateAdd(Src, builder->getInt64(cpu->MemoryMapper->GetBaseOffset<uint64_t>(0)));

    switch (LoadMemOp->Size) {
    case 4:
      Src = builder->CreateIntToPtr(Src, Type::getInt32PtrTy(*con));
    break;
    case 8:
      Src = builder->CreateIntToPtr(Src, Type::getInt64PtrTy(*con));
    break;
    default:
    printf("Unknown LoadSize: %d\n", LoadMemOp->Size);
    std::abort();
    break;
    }
    Values[Offset] = builder->CreateLoad(Src);
#endif
  }
  break;

  default:
    printf("Unknown IR Op: %d(%s)\n", op->Op, Emu::IR::GetName(op->Op).data());
    std::abort();
  break;
  }
}

void LLVM::FindJumpTargets(Emu::IR::IntrusiveIRList const *ir) {
  auto Size = ir->GetOffset();
  uint64_t i = 0;

  //printf("New Jump Target! Block: 0x%zx\n", cpu->ParentThread->CPUState.rip);
  uint64_t LocalRIP = cpu->ParentThread->CPUState.rip;

  // Walk through ops and remember IR Jump Targets
  std::unordered_map<IR::AlignmentType, bool> IRTargets;
  while (i != Size) {
    auto op = ir->GetOp(i);
    if (op->Op == IR::OP_COND_JUMP) {
      auto JumpOp = op->C<IR::IROp_CondJump>();
      IRTargets[JumpOp->Target] = true;
      JumpTargets[JumpOp->RIPTarget] = true;
      printf("\tAdding JumpTarget(COND_TGT): %zx\n", JumpOp->RIPTarget);

    }
    i += Emu::IR::GetSize(op->Op);
  }

  i = 0;
  while (i != Size) {
    auto op = ir->GetOp(i);
    if (op->Op == IR::OP_RIP_MARKER) {
      auto Op = op->C<IR::IROp_RIPMarker>();
      LocalRIP = Op->RIP;
    }
    else if (op->Op == IR::OP_JUMP_TGT) {
      JumpTargets[LocalRIP] = true;
      printf("\tAdding JumpTarget(TGT): %zx\n", LocalRIP);
    }

    // If this IR Op is a jump target
    if (IRTargets.find(i) != IRTargets.end()) {
      JumpTargets[LocalRIP] = true;
      printf("\tAdding JumpTarget(IRTGT): %zx\n", LocalRIP);
    }
    i += Emu::IR::GetSize(op->Op);
  }

}

void* LLVM::CompileCode(Emu::IR::IntrusiveIRList const *ir) {
  using namespace llvm;
  std::string FunctionName = "Function" + std::to_string(cpu->ParentThread->CPUState.rip);
	auto testmodule = new llvm::Module("Main Module", *con);
  auto engine = EngineBuilder(std::unique_ptr<llvm::Module>(testmodule))
		.setEngineKind(EngineKind::JIT)
		.create();

  auto functype = FunctionType::get(Type::getVoidTy(*con), {}, false);
  func = Function::Create(functype,
      Function::ExternalLinkage,
      FunctionName,
      testmodule);

  func->setCallingConv(CallingConv::C);

  llvm::Function *fallback;
  {
    auto functype = FunctionType::get(Type::getVoidTy(*con), {Type::getInt64Ty(*con)}, false);
    fallback = Function::Create(functype,
        Function::ExternalLinkage,
        "Fallback",
        testmodule);

    using ClassPtr = void(Emu::CPUCore::*)(CPUCore::ThreadState*);
    union Test{
      ClassPtr ClassData;
      void* Data;
    };
    Test A;
    A.ClassData = &CPUCore::FallbackToUnicorn;
    engine->addGlobalMapping(fallback, A.Data);
  }

  auto entry = BasicBlock::Create(*con, "entry", func);
  builder->SetInsertPoint(entry);

  CreateGlobalVariables(engine, testmodule);

  // XXX: Finding our jump targets shouldn't be this dumb
  JumpTargets.clear();
  FindJumpTargets(ir);

  auto Size = ir->GetOffset();
  uint64_t i = 0;

  CurrentRIP = cpu->ParentThread->CPUState.rip;
//  printf("New Block: 0x%zx\n", cpu->ParentThread->CPUState.rip);
  while (i != Size) {
    auto op = ir->GetOp(i);
    HandleIR(i, op);
    i += Emu::IR::GetSize(op->Op);
  }

  legacy::PassManager PM;
  PassManagerBuilder PMBuilder;
  PMBuilder.OptLevel = 2;
  raw_ostream& out = outs();
  if (cpu->ParentThread->CPUState.rip == 0x402350)
    PM.add(createPrintModulePass(out));

  verifyModule(*testmodule, &out);
  PMBuilder.populateModulePassManager(PM);
  PM.run(*testmodule);
  engine->finalizeObject();

  functions.emplace_back(engine);
  void *ptr = (void*)engine->getFunctionAddress(FunctionName);
  auto GetTime = []() {
    return std::chrono::high_resolution_clock::now();
  };
  if (cpu->ParentThread->CPUState.rip == 0x402350)
  {
    X86State state;
    memcpy(&state, &cpu->ParentThread->CPUState, sizeof(state));

    using JITPtr = void (*)(CPUCore *);
    JITPtr call = (JITPtr)ptr;
    for (int i = 0; i < 5; ++i) {
      memcpy(&cpu->ParentThread->CPUState, &state, sizeof(state));
      auto start = GetTime();
      call(cpu);
      auto time = GetTime();
      printf("Test from inside app: %zd %zd\n", cpu->ParentThread->CPUState.gregs[REG_RAX], (time - start).count());
    }
    memcpy(&cpu->ParentThread->CPUState, &state, sizeof(state));

    printf("Ptr: %p\n", ptr);
//    std::abort();
  }

  return ptr;

};

CPUBackend *CreateLLVMBackend(Emu::CPUCore *CPU) {
  return new LLVM(CPU);
}

}
