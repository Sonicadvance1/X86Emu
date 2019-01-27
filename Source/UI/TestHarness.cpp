#include "Core/CPU/IntrusiveIRList.h"
#include "Core/CPU/CPUBackend.h"
#include "Core/CPU/CPUCore.h"
#include "ELFLoader.h"
#include "LogManager.h"

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

void MsgHandler(LogMan::DebugLevels Level, std::string const &Message) {
  const char *CharLevel{nullptr};

  switch (Level) {
  case LogMan::NONE:
    CharLevel = "NONE";
    break;
  case LogMan::ASSERT:
    CharLevel = "ASSERT";
    break;
  case LogMan::ERROR:
    CharLevel = "ERROR";
    break;
  case LogMan::DEBUG:
    CharLevel = "DEBUG";
    break;
  case LogMan::INFO:
    CharLevel = "Info";
    break;
  default:
    CharLevel = "???";
    break;
  }
  printf("[%s] %s\n", CharLevel, Message.c_str());
}

void AssertHandler(std::string const &Message) {
  printf("[ASSERT] %s\n", Message.c_str());
}

class LLVMIRVisitor final {
public:
  LLVMIRVisitor();
  ~LLVMIRVisitor();
  void *Visit(Emu::IR::IntrusiveIRList const *ir);

private:
  llvm::LLVMContext *con;
  LLVMContextRef conref;
	llvm::Module *mainmodule;
  llvm::IRBuilder<> *builder;
  llvm::Function *func;
  std::vector<llvm::ExecutionEngine*> functions;
};

LLVMIRVisitor::LLVMIRVisitor() {
	using namespace llvm;
	InitializeNativeTarget();
	InitializeNativeTargetAsmPrinter();
	conref = LLVMContextCreate();
  con = *llvm::unwrap(&conref);
	mainmodule = new llvm::Module("Main Module", *con);
  builder = new IRBuilder<>(*con);
}

LLVMIRVisitor::~LLVMIRVisitor() {
  delete builder;
  for (auto module : functions)
    delete module;
	LLVMContextDispose(conref);
}

void *LLVMIRVisitor::Visit(Emu::IR::IntrusiveIRList const *ir) {
  using namespace llvm;

  std::string FunctionName = "Function";
	auto testmodule = new llvm::Module("Main Module", *con);

	SmallVector<std::string, 0> attrs;
	std::string arch = "";
	std::string cpu = "haswell";

  auto engine_builder = EngineBuilder(std::unique_ptr<llvm::Module>(testmodule));
	engine_builder.setEngineKind(EngineKind::JIT);
	TargetOptions opts;

	Triple test("x86_64", "None", "Unknown");

	TargetMachine* target = engine_builder.selectTarget(
		test,
		arch, cpu, attrs);
	if (target == nullptr) {
		printf("Couldn't select target");
		return nullptr;
	}
	auto engine = engine_builder.create(target);

  auto functype = FunctionType::get(Type::getInt64Ty(*con), {Type::getInt64Ty(*con)}, false);
  func = Function::Create(functype,
      Function::ExternalLinkage,
      FunctionName,
      testmodule);

  func->setCallingConv(CallingConv::C);

  size_t Size = ir->GetOffset();
  size_t i = 0;
  BasicBlock *curblock = nullptr;
  BasicBlock *entryblock = nullptr;
  std::unordered_map<size_t, BasicBlock*> block_locations;
  std::unordered_map<size_t, Value*> Values;

  struct FixupData {
    size_t From;
    BasicBlock *block;
  };
  // Destination
  std::unordered_map<size_t, FixupData> RequiredFixups;
  Value *ContextData;
  while (i != Size) {
    auto op = ir->GetOp(i);
    using namespace Emu::IR;
    switch (op->Op) {
    case OP_BEGINFUNCTION:
    case OP_ENDFUNCTION:
    break;
    case OP_BEGINBLOCK: {
      LogMan::Throw::A(curblock == nullptr, "Oops, hit begin block without ending the other one.");
      curblock = BasicBlock::Create(*con, "block", func);
      block_locations[i] = curblock;
      if (entryblock == nullptr) {
        entryblock = curblock;
      }
      auto it = RequiredFixups.find(i);
      if (it != RequiredFixups.end()) {
        // Something is trying to jump to this location
        // This only happens on forward jumps!
        RequiredFixups.erase(it);
        builder->SetInsertPoint(it->second.block);
        auto FromOp = ir->GetOp(it->second.From);
        switch (FromOp->Op) {
//        case OP_COND_JUMP: {
//          auto Op = op->C<IROp_CondJump>();
//
//        }
//        break;
        case OP_JUMP: {
          auto Op = op->C<IROp_Jump>();
          builder->CreateBr(curblock);
        }
        break;
        default:
          LogMan::Msg::A("Unknown Source inst type!");
        }
      }
      builder->SetInsertPoint(curblock);
    }
    break;
    case OP_ENDBLOCK: {
      curblock = nullptr;
    }
    break;
    case OP_ALLOCATE_CONTEXT: {
      auto Op = op->C<IROp_AllocateContext>();
      ContextData = builder->CreateAlloca(ArrayType::get(Type::getInt64Ty(*con), Op->Size / 8));
    }
    break;
    case OP_STORECONTEXT: {
      auto Op = op->C<IROp_StoreContext>();
      auto location = builder->CreateGEP(ContextData,
          {
            builder->getInt32(0),
            builder->getInt32(Op->Offset / 8),
          },
          "ContextLoad");
      builder->CreateStore(Values[Op->Arg], location);
    }
    break;
    case OP_LOADCONTEXT: {
      auto Op = op->C<IROp_LoadContext>();
      auto location = builder->CreateGEP(ContextData,
          {
            builder->getInt32(0),
            builder->getInt32(Op->Offset / 8),
          },
          "ContextLoad");
      Values[i] = builder->CreateLoad(location);
    }
    break;
    case OP_GETARGUMENT: {
      auto Op = op->C<IROp_GetArgument>();
      Values[i] = func->arg_begin() + Op->Argument;
    }
    break;
    case OP_ADD: {
      auto Op = op->C<IROp_Add>();
      Values[i] = builder->CreateAdd(Values[Op->Args[0]], Values[Op->Args[1]]);
    }
    break;
    case OP_SUB: {
      auto Op = op->C<IROp_Sub>();
      Values[i] = builder->CreateSub(Values[Op->Args[0]], Values[Op->Args[1]]);
    }
    break;
    case OP_SHL: {
      auto Op = op->C<IROp_Shl>();
      Values[i] = builder->CreateShl(Values[Op->Args[0]], Values[Op->Args[1]]);
    }
    break;
    case OP_CONSTANT: {
      auto Op = op->C<IROp_Constant>();
      Values[i] = builder->getInt64(Op->Constant);
    }
    break;
    case OP_LOAD_MEM: {
      auto Op = op->C<IROp_LoadMem>();

      Value *Src;
      switch (Op->Size) {
      case 4:
        Src = builder->CreateIntToPtr(Values[Op->Arg[0]], Type::getInt32PtrTy(*con));
      break;
      case 8:
        Src = builder->CreateIntToPtr(Values[Op->Arg[0]], Type::getInt64PtrTy(*con));
      break;
      default:
        printf("Unknown LoadSize: %d\n", Op->Size);
        std::abort();
      break;
      }
      Values[i] = builder->CreateLoad(Src);
    }
    break;
    case OP_COND_JUMP: {
      auto Op = op->C<IROp_CondJump>();
      if (i > Op->Target) {
        // Conditional backwards jump
        // IR doesn't split the block on the false path
        // LLVM splits the block at all branches
        auto FalsePath = BasicBlock::Create(*con, "false", func);

        auto Comp = builder->CreateICmpNE(Values[Op->Cond], builder->getInt64(0));
        builder->CreateCondBr(Comp, block_locations[Op->Target], FalsePath);
        builder->SetInsertPoint(FalsePath);
        curblock = FalsePath;
        block_locations[i] = curblock;
      }
      else {
        // Conditional forward jump
        FixupData data;
        data.From = i;
        data.block = curblock;
        RequiredFixups[Op->Target] = data;
      }
    }
    break;
    case OP_JUMP: {
      auto Op = op->C<IROp_Jump>();
      if (i > Op->Target) {
        // Backwards jump
        builder->CreateBr(block_locations[Op->Target]);
      }
      else {
        // Forward jump
        FixupData data;
        data.From = i;
        data.block = curblock;
        RequiredFixups[Op->Target] = data;
      }
    }
    break;
    case OP_RETURN: {
      auto Op = op->C<IROp_Return>();
      builder->CreateRet(Values[Op->Arg]);
    }
    break;
    default:
      LogMan::Msg::A("Unknown Op: ", Emu::IR::GetName(op->Op).data());
    break;
    }
    i += Emu::IR::GetSize(op->Op);
  }
  legacy::PassManager PM;
  PassManagerBuilder PMBuilder;
  PMBuilder.OptLevel = 2;
  raw_ostream& out = outs();
  PM.add(createPrintModulePass(out));

  verifyModule(*testmodule, &out);
  PMBuilder.populateModulePassManager(PM);
  PM.run(*testmodule);
  engine->finalizeObject();

  functions.emplace_back(engine);
  void *ptr = (void*)engine->getFunctionAddress(FunctionName);
  return ptr;
}

int main(int argc, char **argv) {
  LogMan::Throw::InstallHandler(AssertHandler);
  LogMan::Msg::InstallHandler(MsgHandler);

  using namespace Emu::IR;
  Emu::IR::IntrusiveIRList ir{8 * 1024 * 1024};
#if 1
  auto func = ir.AllocateOp<IROp_BeginFunction, OP_BEGINFUNCTION>();
  func.first->Arguments = 1;
  func.first->HasReturn = true;

  uint64_t ResultLocation = 0;
  uint64_t DecLocation = 8;


  {
    auto header = ir.AllocateOp<IROp_BeginBlock, OP_BEGINBLOCK>();
    IntrusiveIRList::IRPair<IROp_Jump> header_jump;
    {
      auto result = ir.AllocateOp<IROp_Constant, OP_CONSTANT>();
      result.first->Constant = 0;

      auto context = ir.AllocateOp<IROp_AllocateContext, OP_ALLOCATE_CONTEXT>();
      context.first->Size = 16;

      auto resultstore = ir.AllocateOp<IROp_StoreContext, OP_STORECONTEXT>();
      resultstore.first->Size = 8;
      resultstore.first->Offset = ResultLocation;
      resultstore.first->Arg = result.second;

      auto loop_store = ir.AllocateOp<IROp_StoreContext, OP_STORECONTEXT>();
      loop_store.first->Size = 8;
      loop_store.first->Offset = DecLocation;
      loop_store.first->Arg = result.second;

      header_jump = ir.AllocateOp<IROp_Jump, OP_JUMP>();
    }
    ir.AllocateOp<IROp_EndBlock, OP_ENDBLOCK>();

    auto loop_body = ir.AllocateOp<IROp_BeginBlock, OP_BEGINBLOCK>();
    header_jump.first->Target = loop_body.second;

    IntrusiveIRList::IRPair<IROp_Jump> body_jump;
    {
      auto loop_load = ir.AllocateOp<IROp_LoadContext, OP_LOADCONTEXT>();
      loop_load.first->Size = 8;
      loop_load.first->Offset = DecLocation;

      {
        auto scale_amt = ir.AllocateOp<IROp_Constant, OP_CONSTANT>();
        scale_amt.first->Constant = 3;

        auto arg1 = ir.AllocateOp<IROp_GetArgument, OP_GETARGUMENT>();
        arg1.first->Argument = 0;

        auto loop_multiply = ir.AllocateOp<IROp_Shl, OP_SHL>();
        loop_multiply.first->Args[0] = loop_load.second;
        loop_multiply.first->Args[1] = scale_amt.second;

        auto mem_offset = ir.AllocateOp<IROp_Add, OP_ADD>();
        mem_offset.first->Args[0] = loop_multiply.second;
        mem_offset.first->Args[1] = arg1.second;

        auto loadmem = ir.AllocateOp<IROp_LoadMem, OP_LOAD_MEM>();
        loadmem.first->Size = 8;
        loadmem.first->Arg[0] = mem_offset.second;
        loadmem.first->Arg[1] = ~0;

        auto res_load = ir.AllocateOp<IROp_LoadContext, OP_LOADCONTEXT>();
        res_load.first->Size = 8;
        res_load.first->Offset = ResultLocation;

        auto local_result = ir.AllocateOp<IROp_Add, OP_ADD>();
        local_result.first->Args[0] = loadmem.second;
        local_result.first->Args[1] = res_load.second;

        auto res_store = ir.AllocateOp<IROp_StoreContext, OP_STORECONTEXT>();
        res_store.first->Size = 8;
        res_store.first->Offset = ResultLocation;
        res_store.first->Arg = local_result.second;
      }

      auto inc_amt = ir.AllocateOp<IROp_Constant, OP_CONSTANT>();
      inc_amt.first->Constant = 1;

      auto loop_index = ir.AllocateOp<IROp_Add, OP_ADD>();
      loop_index.first->Args[0] = loop_load.second;
      loop_index.first->Args[1] = inc_amt.second;

      auto loop_store = ir.AllocateOp<IROp_StoreContext, OP_STORECONTEXT>();
      loop_store.first->Size = 8;
      loop_store.first->Offset = DecLocation;
      loop_store.first->Arg = loop_index.second;

      auto loop_count = ir.AllocateOp<IROp_Constant, OP_CONSTANT>();
      loop_count.first->Constant = 10000;

      auto loop_remaining = ir.AllocateOp<IROp_Sub, OP_SUB>();
      loop_remaining.first->Args[0] = loop_count.second;
      loop_remaining.first->Args[1] = loop_index.second;

      auto cond_jump = ir.AllocateOp<IROp_CondJump, OP_COND_JUMP>();
      cond_jump.first->Cond = loop_remaining.second;
      cond_jump.first->Target = loop_body.second;

      body_jump = ir.AllocateOp<IROp_Jump, OP_JUMP>();
    }
    ir.AllocateOp<IROp_EndBlock, OP_ENDBLOCK>();

    auto loop_end = ir.AllocateOp<IROp_BeginBlock, OP_BEGINBLOCK>();
    body_jump.first->Target = loop_end.second;
    {
      auto loop_load = ir.AllocateOp<IROp_LoadContext, OP_LOADCONTEXT>();
      loop_load.first->Size = 8;
      loop_load.first->Offset = ResultLocation;

      auto return_op = ir.AllocateOp<IROp_Return, OP_RETURN>();
      return_op.first->Arg = loop_load.second;
    }
    ir.AllocateOp<IROp_EndBlock, OP_ENDBLOCK>();
  }
  ir.AllocateOp<IROp_EndFunction, OP_ENDFUNCTION>();
#else
  auto func = ir.AllocateOp<IROp_BeginFunction, OP_BEGINFUNCTION>();
  func.first->Arguments = 1;
  func.first->HasReturn = true;
  ir.AllocateOp<IROp_BeginBlock, OP_BEGINBLOCK>();
    auto result = ir.AllocateOp<IROp_Constant, OP_CONSTANT>();
    result.first->Constant = 0xDEADBEEFBAD0DAD1ULL;
    auto return_op = ir.AllocateOp<IROp_Return, OP_RETURN>();
    return_op.first->Arg = result.second;
  ir.AllocateOp<IROp_EndBlock, OP_ENDBLOCK>();
  ir.AllocateOp<IROp_EndFunction, OP_ENDFUNCTION>();
#endif

  ir.Dump();

  LLVMIRVisitor visit;

  using PtrType = uint64_t (*)(uint64_t*);
  PtrType ptr = (PtrType)visit.Visit(&ir);

#define LOOP_SIZE 10000
  std::vector<uint64_t> vals;
  vals.reserve(LOOP_SIZE);
  for (uint64_t i = 0; i < LOOP_SIZE; ++i) {
    vals.emplace_back(i);
  }

	if (ptr) {
		uint64_t ret = ptr(&vals.at(0));
		printf("Ret: %zd\n", ret);
		printf("ptr: 0x%p\n", ptr);
		std::abort();
	}
	else {
		printf("No return pointer passed\n");
	}
  return 0;
}
