#include "Core/CPU/IR.h"
#include "Core/CPU/IntrusiveIRList.h"

namespace Emu::IR {

constexpr std::array<std::string_view const, OP_LASTOP + 1> IRNames = {
	"Constant", // sizeof(IROp_Constant),
	"LoadContext", // sizeof(IROp_LoadContext),
	"StoreContext", // sizeof(IROp_StoreContext),

  // Function management
  "BeginFunction", // sizeof(IROp_BeginFunction),
  "EndFunction", // sizeof(IROp_EndFunction),
  "GetArgument", // sizeof(IROp_GetArgument),
  "AllocateContext", // sizeof(IROp_AllocateContext),

  // Block management
  "BeginBlock", // sizeof(IROp_BeginBlock), // BeginBlock
  "EndBlock", // sizeof(IROp_EndBlock), // EndBlock

  // Branching
  "Jump", // sizeof(IROp_Jump),
  "CondJump", // sizeof(IROp_CondJump),
  "Call", // sizeof(IROp_Call),
  "ExternCall", // sizeof(IROp_ExternCall),
	"Syscall", // sizeof(IROp_Syscall),
  "Return", // sizeof(IROp_Return),

  // Instructions
  "Add", // sizeof(IROp_Add),
  "Sub", // sizeof(IROp_Sub),
  "Or", // sizeof(IROp_Or),
  "Xor", // sizeof(IROp_Xor),
  "Shl", // sizeof(IROp_Shl),
  "Shr", // sizeof(IROp_Shr),
  "And", // sizeof(IROp_And),
  "Nand", // sizeof(IROp_Nand),
  "BitExtract", // sizeof(IROp_BitExtract),
  "Select", // sizeof(IROp_Select),
  "Trunc_32", // sizeof(IROp_Trunc_32),
  "Trunc_16", // sizeof(IROp_Trunc_16),

  // Memory
  "LoadMem", // sizeof(IROp_LoadMem),

  // Misc
  "JmpTarget", // sizeof(IROp_JmpTarget),
	"RIPMarker", // sizeof(IROp_RIPMarker),
  "END",
};

static_assert(IRNames[OP_LASTOP] == "END");

std::string_view const& GetName(IROps Op) {
  return IRNames[Op];
}

void DumpConstantOp(size_t Offset, IROp_Header const *op) {
  auto ConstantOp = op->C<IROp_Constant>();
  printf("%%%zd = %s 0x%zx\n", Offset, GetName(op->Op).data(), ConstantOp->Constant);
}

void DumpLoadContextOp(size_t Offset, IROp_Header const *op) {
  auto LoadContextOp = op->C<IROp_LoadContext>();
  printf("%%%zd = %s 0x%x\n", Offset, GetName(op->Op).data(), LoadContextOp->Offset);
}

void DumpStoreContextOp(size_t Offset, IROp_Header const *op) {
  auto StoreContextOp = op->C<IROp_StoreContext>();
  printf("%s 0x%x\n", GetName(op->Op).data(), StoreContextOp->Offset);
}

void DumpBeginBlockOp(size_t Offset, IROp_Header const *op) {
  auto BeginBlockOp = op->C<IROp_BeginBlock>();
  printf("%s\n", GetName(op->Op).data());
}

void DumpEndBlockOp(size_t Offset, IROp_Header const *op) {
  auto EndBlockOp = op->C<IROp_EndBlock>();
  printf("%s %zd\n", GetName(op->Op).data(), EndBlockOp->RIPIncrement);
}

void DumpBinOp(size_t Offset, IROp_Header const *op) {
  auto BinOp = op->C<IROp_BiOp>();
  printf("%%%zd = %s %%%d %%%d\n", Offset, GetName(op->Op).data(), BinOp->Args[0], BinOp->Args[1]);
}

void DumpLoadMemOp(size_t Offset, IROp_Header const *op) {
  auto LoadMemOp = op->C<IROp_LoadMem>();
  printf("%%%zd = %s [%%%d", Offset, GetName(op->Op).data(), LoadMemOp->Arg[0]);
  if (LoadMemOp->Arg[1] != ~0) {
    printf(" + %%%d]\n", LoadMemOp->Arg[1]);
  }
  else {
    printf("]\n");
  }
}

void DumpCondJump(size_t Offset, IROp_Header const *op) {
  auto CondJump = op->C<IROp_CondJump>();
  printf("%s %%%d %%%d\n", GetName(op->Op).data(), CondJump->Cond, CondJump->Target);
}

void DumpJmpTarget(size_t Offset, IROp_Header const *op) {
  auto JmpTarget = op->C<IROp_JmpTarget>();
  printf("%%%zd: %s\n", Offset, GetName(op->Op).data());
}

void DumpSyscall(size_t Offset, IROp_Header const *op) {
  auto Syscall = op->C<IROp_Syscall>();
  printf("%s", GetName(op->Op).data());
  for (int i = 0; i < 7; ++i)
    printf(" %%%d", Syscall->Arguments[i]);
  printf("\n");
}

void DumpRIPMarker(size_t Offset, IROp_Header const *op) {
  auto RIPMarker = op->C<IROp_RIPMarker>();
  printf("%s 0x%zx\n", GetName(op->Op).data(), RIPMarker->RIP);
}

void DumpInvalid(size_t Offset, IROp_Header const *op) {
  printf("%zd Invalid %s\n", Offset, GetName(op->Op).data());
}

using OpDumpVisitor = void (*)(size_t, IROp_Header const*);
constexpr std::array<OpDumpVisitor, OP_LASTOP + 1> IRDump = {
	DumpConstantOp, // sizeof(IROp_Constant),
	DumpLoadContextOp, // sizeof(IROp_LoadContext),
	DumpStoreContextOp, // sizeof(IROp_StoreContext),

  // Function Management
  DumpInvalid, // sizeof(IROp_BeginFunction),
  DumpInvalid, // sizeof(IROp_EndFunction),
  DumpInvalid, // sizeof(IROp_GetArgument),
  DumpInvalid, // sizeof(IROp_AllocateContext),

  // Block Management
  DumpBeginBlockOp, // sizeof(IROp_BeginBlock), // BeginBlock
  DumpEndBlockOp, // sizeof(IROp_EndBlock), // EndBlock

  // Branching
  DumpInvalid, // sizeof(IROp_Jump),
  DumpCondJump, // sizeof(IROp_CondJump),
  DumpInvalid, // sizeof(IROp_Call),
  DumpInvalid, // sizeof(IROp_ExternCall),
	DumpSyscall, // sizeof(IROp_Syscall),
  DumpInvalid, // sizeof(IROp_Return),

  // Instructions
  DumpBinOp, // sizeof(IROp_Add),
  DumpBinOp, // sizeof(IROp_Sub),
  DumpBinOp, // sizeof(IROp_Or),
  DumpBinOp, // sizeof(IROp_Xor),
  DumpBinOp, // sizeof(IROp_Shl),
  DumpBinOp, // sizeof(IROp_Shr),
  DumpBinOp, // sizeof(IROp_And),
  DumpBinOp, // sizeof(IROp_Nand),
  DumpBinOp, // sizeof(IROp_BitExtract),
  DumpBinOp, // sizeof(IROp_Select),
  DumpBinOp, // sizeof(IROp_Trunc_32),
  DumpBinOp, // sizeof(IROp_Trunc_16),

  // Memory
  DumpLoadMemOp, // sizeof(IROp_LoadMem),

  // Misc
  DumpJmpTarget, // sizeof(IROp_JmpTarget),
	DumpRIPMarker, // sizeof(IROp_RIPMarker),
  DumpInvalid,
};

static_assert(IRDump[OP_LASTOP] == DumpInvalid);

void VisitOp(size_t Offset, IROp_Header const* op) {
  IRDump[op->Op](Offset, op);
}

void Dump(IntrusiveIRList const* IR) {
  size_t Size = IR->GetOffset();

  size_t i = 0;
  while (i != Size) {
    auto op = IR->GetOp(i);
    VisitOp(i, op);
    i += GetSize(op->Op);
  }
}

}
