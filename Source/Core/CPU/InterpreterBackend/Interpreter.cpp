#include "Core/CPU/CPUCore.h"
#include "Core/CPU/IR.h"
#include "Core/CPU/IntrusiveIRList.h"
#include "Interpreter.h"
#include "LogManager.h"
#include <map>

namespace Emu {

static void TestCompilation(CPUCore *cpu) {
  auto threadstate = cpu->GetTLSThread();
  auto IR = cpu->GetIRList(threadstate, threadstate->CPUState.rip);
  auto Size = IR->GetOffset();

  size_t i = 0;
  // XXX: Change this map over to a register bank that supports real RA
  // Works now for testing
  std::map<IR::AlignmentType, uint64_t> Values;

  bool End = false;
  while (i != Size && !End) {
    auto op = IR->GetOp(i);
    size_t opSize = Emu::IR::GetSize(op->Op);

    switch (op->Op) {
    case IR::OP_BEGINBLOCK:
    break;
    case IR::OP_JUMP_TGT:
      printf("Landed on jump target\n");
    break;
    case IR::OP_ENDBLOCK: {
      auto EndOp = op->C<IR::IROp_EndBlock>();
      threadstate->CPUState.rip += EndOp->RIPIncrement;
      // If we hit an end block that isn't at the end of the stream that means we need to early exit
      // Just set ourselves to the end regardless
      End = true;
    break;
    }
    case IR::OP_COND_JUMP: {
      auto JumpOp = op->C<IR::IROp_CondJump>();
      if (!!Values[JumpOp->Cond])
        i = JumpOp->Target - opSize;
    }
    break;
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

      uintptr_t ContextPtr = reinterpret_cast<uintptr_t>(&threadstate->CPUState);
      ContextPtr += LoadOp->Offset;

      uint64_t *ContextData = reinterpret_cast<uint64_t*>(ContextPtr);
      Values[i] = *ContextData;
    break;
    }
    case IR::OP_STORECONTEXT: {
      auto StoreOp = op->C<IR::IROp_StoreContext>();
      LogMan::Throw::A(StoreOp->Size == 8, "Can only handle 8 byte");

      uintptr_t ContextPtr = reinterpret_cast<uintptr_t>(&threadstate->CPUState);
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
    case IR::OP_SUB: {
      auto SubOp = op->C<IR::IROp_Sub>();
      Values[i] = Values[SubOp->Args[0]] - Values[SubOp->Args[1]];
    }
    break;
    case IR::OP_OR: {
      auto OrOp = op->C<IR::IROp_Or>();
      Values[i] = Values[OrOp->Args[0]] | Values[OrOp->Args[1]];
    }
    break;
    case IR::OP_XOR: {
      auto XorOp = op->C<IR::IROp_Xor>();
      Values[i] = Values[XorOp->Args[0]] ^ Values[XorOp->Args[1]];
    }
    break;
    case IR::OP_SHL: {
      auto ShlOp = op->C<IR::IROp_Shl>();
      Values[i] = Values[ShlOp->Args[0]] << Values[ShlOp->Args[1]];
    }
    break;
    case IR::OP_SHR: {
      auto ShrOp = op->C<IR::IROp_Shr>();
      Values[i] = Values[ShrOp->Args[0]] >> Values[ShrOp->Args[1]];
    }
    break;
    case IR::OP_AND: {
      auto AndOp = op->C<IR::IROp_And>();
      Values[i] = Values[AndOp->Args[0]] & Values[AndOp->Args[1]];
    }
    break;
    case IR::OP_NAND: {
      auto NandOp = op->C<IR::IROp_Nand>();
      Values[i] = Values[NandOp->Args[0]] & ~(Values[NandOp->Args[1]]);
    }
    break;
    case IR::OP_BITEXTRACT: {
      auto BitExtractOp = op->C<IR::IROp_BitExtract>();
      Values[i] = (Values[BitExtractOp->Args[0]] >> Values[BitExtractOp->Args[1]]) & 1;
    }
    break;
    case IR::OP_SELECT: {
      auto SelectOp = op->C<IR::IROp_Select>();
      switch (SelectOp->Op) {
      case IR::IROp_Select::COMP_EQ:
        Values[i] = Values[SelectOp->Args[0]] == Values[SelectOp->Args[1]] ?
          Values[SelectOp->Args[2]] : Values[SelectOp->Args[3]];
        break;
      case IR::IROp_Select::COMP_NEQ:
        Values[i] = Values[SelectOp->Args[0]] != Values[SelectOp->Args[1]] ?
          Values[SelectOp->Args[2]] : Values[SelectOp->Args[3]];
        break;

      };
    }
    break;

    case IR::OP_TRUNC_32: {
      auto Trunc_32Op = op->C<IR::IROp_Trunc_32>();
      Values[i] = Values[Trunc_32Op->Arg] & 0xFFFFFFFFULL;
    }
    break;
    case IR::OP_TRUNC_16: {
      auto Trunc_16Op = op->C<IR::IROp_Trunc_16>();
      Values[i] = Values[Trunc_16Op->Arg] & 0x0000FFFFULL;
    }
    break;
    case IR::OP_LOAD_MEM: {
      auto LoadMemOp = op->C<IR::IROp_LoadMem>();
      uint64_t Src = Values[LoadMemOp->Arg[0]];
      if (LoadMemOp->Arg[1] != ~0)
        Src += Values[LoadMemOp->Arg[1]];

      void *ptr = cpu->MemoryMapper->GetPointer(Src);
      switch (LoadMemOp->Size) {
      case 4:
        printf("32bit load from %zx(%d)\n", Src, *(uint32_t*)ptr);
        Values[i] = *(uint32_t*)ptr;
      break;
      case 8:
        Values[i] = *(uint64_t*)ptr;
      break;
      default:
      printf("Unknown LoadSize: %d\n", LoadMemOp->Size);
      std::abort();
      break;
      }
    }
    break;
    default:
      printf("Unknown IR Op: %d(%s)\n", op->Op, Emu::IR::GetName(op->Op).data());
      std::abort();
    break;
    }

    i += opSize;
  }
}

void* Interpreter::CompileCode(Emu::IR::IntrusiveIRList const *ir) {
  return (void*)TestCompilation;
};
}
