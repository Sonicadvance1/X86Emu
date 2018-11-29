#include "Core/CPU/CPUCore.h"
#include "Core/CPU/IR.h"
#include "Core/CPU/IntrusiveIRList.h"
#include "Interpreter.h"
#include "LogManager.h"
#include <map>

namespace Emu {

static void TestCompilation(CPUCore *cpu) {
  auto IR = cpu->GetIRList(cpu->CPUState.rip);
  auto Size = IR->GetOffset();

  size_t i = 0;
  // XXX: Change this map over to a register bank that supports real RA
  // Works now for testing
  std::map<IR::AlignmentType, uint64_t> Values;

  while (i != Size) {
    auto op = IR->GetOp(i);

    switch (op->Op) {
    case IR::OP_BEGINBLOCK:
    break;
    case IR::OP_ENDBLOCK: {
      auto EndOp = op->C<IR::IROp_EndBlock>();
      cpu->CPUState.rip += EndOp->RIPIncrement;
    break;
    }
    case IR::OP_CONSTANT: {
      auto ConstantOp = op->C<IR::IROp_Constant>();
      Values[i] = ConstantOp->Constant;
    }
    break;
    case IR::OP_SYSCALL: {
      auto SyscallOp = op->C<IR::IROp_Syscall>();

      Emu::SyscallHandler::SyscallArguments Args;
      for (int i = 0; i < IR::IROp_Syscall::MAX_ARGS; ++i)
        Args.Argument[i] = Values[SyscallOp->Arguments[i]];

      uint64_t Res = cpu->syscallhandler.HandleSyscall(&Args);
      Values[i] = Res;
    break;
    }
    case IR::OP_LOADCONTEXT: {
      auto LoadOp = op->C<IR::IROp_LoadContext>();
      LogMan::Throw::A(LoadOp->Size == 8, "Can only handle 8 byte");

      uintptr_t ContextPtr = reinterpret_cast<uintptr_t>(&cpu->CPUState);
      ContextPtr += LoadOp->Offset;

      uint64_t *ContextData = reinterpret_cast<uint64_t*>(ContextPtr);
      Values[i] = *ContextData;
    break;
    }
    case IR::OP_STORECONTEXT: {
      auto StoreOp = op->C<IR::IROp_StoreContext>();
      LogMan::Throw::A(StoreOp->Size == 8, "Can only handle 8 byte");

      uintptr_t ContextPtr = reinterpret_cast<uintptr_t>(&cpu->CPUState);
      ContextPtr += StoreOp->Offset;

      uint64_t *ContextData = reinterpret_cast<uint64_t*>(ContextPtr);
      *ContextData = Values[StoreOp->Arg];
    }
    break;
    case IR::OP_ADD: {
      auto AddOp = op->C<IR::IROp_Add>();
      Values[i] = Values[AddOp->Args[0]] + Values[AddOp->Args[1]];
    }
    break;
    case IR::OP_XOR: {
      auto XorOp = op->C<IR::IROp_Xor>();
      Values[i] = Values[XorOp->Args[0]] ^ Values[XorOp->Args[1]];
    }
    break;
    case IR::OP_TRUNC_32: {
      auto Trunc_32Op = op->C<IR::IROp_Trunc_32>();
      Values[i] = Values[Trunc_32Op->Arg] & 0xFFFFFFFFULL;
    }
    break;

    default:
      printf("Unknown IR Op: %d(%s)\n", op->Op, Emu::IR::GetName(op->Op).c_str());
      std::abort();
    break;
    }

    i += Emu::IR::GetSize(op->Op);
  }
}

void* Interpreter::CompileCode(Emu::IR::IntrusiveIRList const *ir) {
  return (void*)TestCompilation;
};
}
