#include "Core/CPU/CPUCore.h"
#include "Core/CPU/X86Tables.h"
#include "LogManager.h"
#include "OpcodeDispatch.h"
#include <cstddef>
#include <iomanip>
#include <vector>

#define DISABLE_DECODE() do { DecodeFailure = true; return; } while(0)
namespace Emu::IR {
static uint8_t GetModRM_Mod(uint8_t modrm) {
  return (modrm & 0b11000000) >> 6;
}
static uint8_t GetModRM_Reg(uint8_t modrm) {
  return (modrm & 0b00111000) >> 3;
}
static uint8_t GetModRM_RM(uint8_t modrm) {
  return modrm & 0b111;
}
static uint8_t GetREX_W(uint8_t rex) {
  return (rex & 0b1000) >> 3;
}
static uint8_t GetREX_R(uint8_t rex) {
  return (rex & 0b0100) >> 2;
}
static uint8_t GetREX_X(uint8_t rex) {
  return (rex & 0b0010) >> 1;
}
static uint8_t GetREX_B(uint8_t rex) {
  return rex & 0b0001;
}

static uint32_t MapModRMToReg(uint8_t bits) {
  std::array<uint64_t, 8> GPRIndexes = {
    REG_RAX,
    REG_RCX,
    REG_RDX,
    REG_RBX,
    REG_RSP,
    REG_RBP,
    REG_RSI,
    REG_RDI,
  };
  return GPRIndexes[bits];
}

static uint32_t MapModRMToReg(uint8_t REX, uint8_t bits) {
  std::array<uint64_t, 16> GPRIndexes = {
    // Classical ordering?
    REG_RAX,
    REG_RCX,
    REG_RDX,
    REG_RBX,
    REG_RSP,
    REG_RBP,
    REG_RSI,
    REG_RDI,
    REG_R8,
    REG_R9,
    REG_R10,
    REG_R11,
    REG_R12,
    REG_R13,
    REG_R14,
    REG_R15,
  };
  return GPRIndexes[(REX << 3) | bits];
}

static std::string RegToString(uint32_t Reg) {
  switch(Reg) {
    case REG_RAX: return "RAX";
    case REG_RCX: return "RCX";
    case REG_RDX: return "RDX";
    case REG_RBX: return "RBX";
    case REG_RSP: return "RSP";
    case REG_RBP: return "RBP";
    case REG_RSI: return "RSI";
    case REG_RDI: return "RDI";
    case REG_R8: return "R8";
    case REG_R9: return "R9";
    case REG_R10: return "R10";
    case REG_R11: return "R11";
    case REG_R12: return "R12";
    case REG_R13: return "R13";
    case REG_R14: return "R14";
    case REG_R15: return "R15";
    default: return "UNK";
  };
}

void OpDispatchBuilder::BeginBlock() {
  IRList.AllocateOp<IROp_BeginBlock, OP_BEGINBLOCK>();
}

void OpDispatchBuilder::EndBlock(uint64_t RIPIncrement) {
  auto EndOp = IRList.AllocateOp<IROp_EndBlock, OP_ENDBLOCK>();
  EndOp.first->RIPIncrement = RIPIncrement;
}

void OpDispatchBuilder::AddOp(Emu::X86Tables::DecodedOp Op, uint8_t const *Code) {
  uint32_t DestReg = 0;

  if (!(Op.second.Flags & X86Tables::DECODE_FLAG_MODRM))
    DecodeFailure = true;
  if (Op.second.Flags & X86Tables::DECODE_FLAG_SIB)
    DecodeFailure = true;

  uint8_t REX = 0;
  uint8_t ModRM = 0;

  uint32_t OpSize = 4;
  AlignmentType Src;

  if (Op.second.Flags & X86Tables::DECODE_FLAG_OPSIZE) {
    printf("Add OpSize\n");
    OpSize = 2;
  }

  if (Op.second.Flags & X86Tables::DECODE_FLAG_REX) {
    REX = Code[0];
    ModRM = Code[2];

    if (GetREX_W(REX)) {
      OpSize = 8;
    }
  } else {
    ModRM = Code[1];
  }

  uint8_t Mod = GetModRM_Mod(ModRM);
  uint8_t RM = GetModRM_RM(ModRM);
  uint8_t Reg = GetModRM_Reg(ModRM);
  if (Mod != 0b11 && Reg != 0b100) {
    DestReg = MapModRMToReg(GetREX_R(REX), GetModRM_Reg(ModRM));
//    printf("XXX: ADD SIB ModRM without Byte!!Mod: %d Src: %s Base? :%s\n",
//        Mod,
//        RegToString(MapModRMToReg(GetREX_R(REX), GetModRM_Reg(ModRM))).c_str(),
//        RegToString(MapModRMToReg(GetREX_B(REX), GetModRM_RM(ModRM))).c_str()
//        );
    if (Mod == 0b10) {
      // [Register + Displacement32]
//      printf("\tXXX: R+disp32\n");
      DecodeFailure = true;

      uint32_t SrcReg = MapModRMToReg(GetREX_B(REX), GetModRM_RM(ModRM));
      Src = LoadContext(offsetof(X86State, gregs) + SrcReg * 8, 8);

      uint64_t Const = *(int32_t*)&Code[Op.second.Size - 4];
      auto ConstantOp = IRList.AllocateOp<IROp_Constant, OP_CONSTANT>();

      ConstantOp.first->Flags = IR::TYPE_I64;
      ConstantOp.first->Constant = Const;

      auto AddOp = IRList.AllocateOp<IROp_Add, OP_ADD>();
      AddOp.first->Args[0] = Src;
      AddOp.first->Args[1] = ConstantOp.second;

      Src = AddOp.second;
    }
    else if (Mod == 0b00) {
      // [Register]
      printf("\tXXX: R\n");
      //      DecodeFailure = true;
      uint32_t SrcReg = MapModRMToReg(GetREX_B(REX), GetModRM_RM(ModRM));
      Src = LoadContext(offsetof(X86State, gregs) + SrcReg * 8, 8);
      auto LoadMemOp = IRList.AllocateOp<IROp_LoadMem, OP_LOAD_MEM>();

      LoadMemOp.first->Size = OpSize;
      LoadMemOp.first->Arg[0] = Src;
      LoadMemOp.first->Arg[1] = ~0;
      Src = LoadMemOp.second;
    }
    else {
      printf("\tEarlier Mod: %d Reg: %d RM: %d\n", Mod, Reg, RM);
      DecodeFailure = true;
    }
  }
  else if (Mod == 0b11) {
    DestReg = MapModRMToReg(GetREX_B(REX), GetModRM_RM(ModRM));
    uint32_t SrcReg = MapModRMToReg(GetREX_R(REX), GetModRM_Reg(ModRM));
    auto LoadOp = IRList.AllocateOp<IROp_LoadContext, OP_LOADCONTEXT>();
    LoadOp.first->Size = 8;
    LoadOp.first->Offset = offsetof(X86State, gregs) + SrcReg * 8;
    Src = LoadOp.second;
  }
  else {
    printf("\tLast Dec\n");
    DecodeFailure = true;
  }

  if (DecodeFailure)
    return;


  auto LoadDestOp = IRList.AllocateOp<IROp_LoadContext, OP_LOADCONTEXT>();
  LoadDestOp.first->Size = 8;
  LoadDestOp.first->Offset = offsetof(X86State, gregs) + DestReg * 8;

  auto Src1 = LoadDestOp.second;
  auto Src2 = Src;
  switch (OpSize) {
  case 1: LogMan::Msg::A("Unhandled Add size 1"); break;
  case 2: {
    auto Trunc1 = IRList.AllocateOp<IROp_Trunc_16, OP_TRUNC_16>();
    Trunc1.first->Arg = Src1;

    auto Trunc2 = IRList.AllocateOp<IROp_Trunc_16, OP_TRUNC_16>();
    Trunc2.first->Arg = Src2;

    Src1 = Trunc1.second;
    Src2 = Trunc2.second;
  }
  break;
  case 4: {
    auto Trunc1 = IRList.AllocateOp<IROp_Trunc_32, OP_TRUNC_32>();
    Trunc1.first->Arg = Src1;

    auto Trunc2 = IRList.AllocateOp<IROp_Trunc_32, OP_TRUNC_32>();
    Trunc2.first->Arg = Src2;

    Src1 = Trunc1.second;
    Src2 = Trunc2.second;
  }
  break;
  case 8: break;
  }

  auto AddOp = IRList.AllocateOp<IROp_Add, OP_ADD>();
  AddOp.first->Args[0] = Src1;
  AddOp.first->Args[1] = Src2;

  auto Res = AddOp.second;

  switch (OpSize) {
  case 1: LogMan::Msg::A("Unhandled Add size 1"); break;
  case 2: {
    auto Trunc = IRList.AllocateOp<IROp_Trunc_16, OP_TRUNC_16>();
    Trunc.first->Arg = Res;

    Res = Trunc.second;
  }
  break;
  case 4: {
    auto Trunc = IRList.AllocateOp<IROp_Trunc_32, OP_TRUNC_32>();
    Trunc.first->Arg = Res;

    Res = Trunc.second;
  }
  break;
  case 8: break;
  }

  auto StoreOp = IRList.AllocateOp<IROp_StoreContext, OP_STORECONTEXT>();
  StoreOp.first->Size = 8;
  StoreOp.first->Offset = offsetof(X86State, gregs) + DestReg * 8;
  StoreOp.first->Arg = Res;

//  printf("AddSize: %d %02x %02x %02x %02x\n", OpSize,
//      Code[0], Code[1], Code[2], Code[3]);
}

void OpDispatchBuilder::CMPOp(Emu::X86Tables::DecodedOp Op, uint8_t const *Code) {
  uint32_t SrcReg = 0;
  uint32_t DestReg = 0;

  if (!(Op.second.Flags & X86Tables::DECODE_FLAG_MODRM))
    DecodeFailure = true;
  if (Op.second.Flags & X86Tables::DECODE_FLAG_SIB)
    DecodeFailure = true;

  uint8_t REX = 0;
  uint8_t ModRM = 0;

  uint32_t OpSize = 4;

  if (Op.second.Flags & X86Tables::DECODE_FLAG_OPSIZE) {
    printf("Add OpSize\n");
    OpSize = 2;
  }

  if (Op.second.Flags & X86Tables::DECODE_FLAG_REX) {
    REX = Code[0];
    ModRM = Code[2];

    if (GetREX_W(REX)) {
      OpSize = 8;
    }
  } else {
    ModRM = Code[1];
  }

  uint8_t Mod = GetModRM_Mod(ModRM);
  uint8_t RM = GetModRM_RM(ModRM);
  uint8_t Reg = GetModRM_Reg(ModRM);
  if (Mod == 0b11) {
    DestReg = MapModRMToReg(GetREX_B(REX), GetModRM_RM(ModRM));
    SrcReg = MapModRMToReg(GetREX_R(REX), GetModRM_Reg(ModRM));
  }
  else {
    DecodeFailure = true;
  }

  if (cpu->GetTLSThread()->JITRIP != 0x402366)
    DecodeFailure = true;

  if (DecodeFailure)
    return;

  auto Src = LoadContext(offsetof(X86State, gregs) + SrcReg * 8, 8);
  auto Dest = LoadContext(offsetof(X86State, gregs) + DestReg * 8, 8);

  auto SubOp = IRList.AllocateOp<IROp_Sub, OP_SUB>();
  SubOp.first->Args[0] = Dest;
  SubOp.first->Args[1] = Src;

  printf("\t In CMP! %s, %s\n", RegToString(DestReg).c_str(), RegToString(SrcReg).c_str());

  // Sets OF, SF, ZF, AF, PF, CF
  // JNE depends on ZF
  if (1) {
    // Set ZF
    auto ZeroConstant = IRList.AllocateOp<IROp_Constant, OP_CONSTANT>();
    ZeroConstant.first->Flags = IR::TYPE_I64;
    ZeroConstant.first->Constant = 1;

    auto OneConstant = IRList.AllocateOp<IROp_Constant, OP_CONSTANT>();
    OneConstant.first->Flags = IR::TYPE_I64;
    OneConstant.first->Constant = 0;

    auto SelectOp = IRList.AllocateOp<IROp_Select, OP_SELECT>();
    SelectOp.first->Op = IROp_Select::COMP_EQ;
    SelectOp.first->Args[0] = Src;
    SelectOp.first->Args[1] = Dest;
    SelectOp.first->Args[2] = OneConstant.second;
    SelectOp.first->Args[3] = ZeroConstant.second;
    SetZF(SelectOp.second);
  }
}

template<uint32_t Type>
void OpDispatchBuilder::JccOp(Emu::X86Tables::DecodedOp Op, uint8_t const *Code) {
  if (Op.second.Flags & X86Tables::DECODE_FLAG_SIB)
    DecodeFailure = true;

  uint32_t FlagBit = 0;
  bool Negate = false;

  switch (Type) {
  case CC_NOF:
    Negate = true;
  case CC_OF:
    FlagBit = 11;
  break;
  case CC_NC:
    Negate = true;
  case CC_C:
    FlagBit = 0;
  break;

  case CC_NZ:
    Negate = true;
  case CC_Z:
    FlagBit = 6;
  break;

  case CC_NS:
    Negate = true;
  case CC_S:
    FlagBit = 7;
  break;

  case CC_NP:
    Negate = true;
  case CC_P:
    FlagBit = 2;
  break;

  case CC_NBE:
  case CC_BE:
  case CC_NL:
  case CC_L:
  case CC_NLE:
  case CC_LE:
//    printf("\tCouldn't handle this jmp type: %d\n", Type);
    DecodeFailure = true;
    break;
  }

  if (cpu->GetTLSThread()->CPUState.rip != 0x402350)
    DecodeFailure = true;

  if (DecodeFailure)
    return;
  printf("We hit a real jump!\n");

  auto FlagBitOp = GetFlagBit(FlagBit, !Negate);

  auto JumpOp = IRList.AllocateOp<IROp_CondJump, OP_COND_JUMP>();
  JumpOp.first->Cond = FlagBitOp;

  // If condition holds true
  // Set RIP and end block. Else jump over
  uint64_t RIPTarget = 0;

  // False block
  {
    uint64_t ConstLocation = *(int8_t*)&Code[Op.second.Size - 1];
    RIPTarget = cpu->GetTLSThread()->JITRIP + ConstLocation + Op.second.Size;
    JumpOp.first->RIPTarget = RIPTarget;

    auto ConstantOp = IRList.AllocateOp<IROp_Constant, OP_CONSTANT>();
    ConstantOp.first->Flags = IR::TYPE_I64;
    ConstantOp.first->Constant = RIPTarget;
    printf("\tWanting to jump to 0x%zx\n", ConstantOp.first->Constant);

    auto StoreOp = IRList.AllocateOp<IROp_StoreContext, OP_STORECONTEXT>();
    StoreOp.first->Size = 8;
    StoreOp.first->Offset = offsetof(X86State, rip);
    StoreOp.first->Arg = ConstantOp.second;
    StoreContext(ConstantOp.second, offsetof(X86State, rip), 8);

    EndBlock(0);
  }

  auto TargetOp = IRList.AllocateOp<IROp_JmpTarget, OP_JUMP_TGT>();
  JumpOp.first->Target = TargetOp.second;

}

void OpDispatchBuilder::RETOp(Emu::X86Tables::DecodedOp Op, uint8_t const *Code) {

  auto LoadOp = IRList.AllocateOp<IROp_LoadContext, OP_LOADCONTEXT>();
  LoadOp.first->Size = 8;
  LoadOp.first->Offset = offsetof(X86State, gregs[REG_RSP]);

  auto EightConstant = IRList.AllocateOp<IROp_Constant, OP_CONSTANT>();
  EightConstant.first->Flags = IR::TYPE_I64;
  EightConstant.first->Constant = 8;

  auto LoadMemOp = IRList.AllocateOp<IROp_LoadMem, OP_LOAD_MEM>();
  LoadMemOp.first->Size = 8;
  LoadMemOp.first->Arg[0] = LoadOp.second;
  LoadMemOp.first->Arg[1] = ~0;

  auto AddOp = IRList.AllocateOp<IROp_Add, OP_ADD>();
  AddOp.first->Args[0] = LoadOp.second;
  AddOp.first->Args[1] = EightConstant.second;

  // Store new stack pointer
  {
    auto StoreOp = IRList.AllocateOp<IROp_StoreContext, OP_STORECONTEXT>();
    StoreOp.first->Size = 8;
    StoreOp.first->Offset = offsetof(X86State, gregs[REG_RSP]);
    StoreOp.first->Arg = AddOp.second;
  }

  // Store new RIP
  {
    auto StoreOp = IRList.AllocateOp<IROp_StoreContext, OP_STORECONTEXT>();
    StoreOp.first->Size = 8;
    StoreOp.first->Offset = offsetof(X86State, rip);
    StoreOp.first->Arg = LoadMemOp.second;
  }

}

void OpDispatchBuilder::AddImmOp(Emu::X86Tables::DecodedOp Op, uint8_t const *Code) {
  auto LoadOp = IRList.AllocateOp<IROp_LoadContext, OP_LOADCONTEXT>();
  LoadOp.first->Size = 8;
  LoadOp.first->Offset = offsetof(X86State, gregs[REG_RAX]);

  auto ConstantOp = IRList.AllocateOp<IROp_Constant, OP_CONSTANT>();

  if (Op.second.Flags & X86Tables::DECODE_FLAG_REX) {
    ConstantOp.first->Flags = IR::TYPE_I64;
    ConstantOp.first->Constant = *(int32_t*)&Code[Op.second.Size - 4];
  }
  else if (Op.second.Flags & X86Tables::DECODE_FLAG_OPSIZE) {
    ConstantOp.first->Flags = IR::TYPE_I16;
    ConstantOp.first->Constant = *(int16_t*)&Code[Op.second.Size - 2];
  }
  else {
    ConstantOp.first->Flags = IR::TYPE_I32;
    ConstantOp.first->Constant = *(int32_t*)&Code[Op.second.Size - 4];
  }

  auto AddImmOp = IRList.AllocateOp<IROp_Add, OP_ADD>();
  AddImmOp.first->Args[0] = LoadOp.second;
  AddImmOp.first->Args[1] = ConstantOp.second;

  auto StoreOp = IRList.AllocateOp<IROp_StoreContext, OP_STORECONTEXT>();
  StoreOp.first->Size = 8;
  StoreOp.first->Offset = offsetof(X86State, gregs[REG_RAX]);
  StoreOp.first->Arg = AddImmOp.second;
}

void OpDispatchBuilder::AddImmModRMOp(Emu::X86Tables::DecodedOp Op, uint8_t const *Code) {
  uint32_t DestReg = 0;

  if (!(Op.second.Flags & X86Tables::DECODE_FLAG_MODRM))
    DecodeFailure = true;
  if (Op.second.Flags & X86Tables::DECODE_FLAG_SIB)
    DecodeFailure = true;

  uint8_t REX = 0;
  uint8_t ModRM = 0;
  uint32_t OpSize = 4;

  if (Op.second.Flags & X86Tables::DECODE_FLAG_OPSIZE) {
    OpSize = 2;
  }

  if (Op.second.Flags & X86Tables::DECODE_FLAG_REX) {
    REX = Code[0];
    ModRM = Code[2];

    if (GetModRM_Mod(ModRM) != 0b11)
      DecodeFailure = true;
    OpSize = 8;
  } else {
    ModRM = Code[1];

    if (GetModRM_Mod(ModRM) != 0b11)
      DecodeFailure = true;
  }

  DestReg = MapModRMToReg(GetREX_B(REX), GetModRM_RM(ModRM));

  uint64_t Const = *(int8_t*)&Code[Op.second.Size - 1];
  if (DecodeFailure)
    return;

  auto Src = LoadContext(offsetof(X86State, gregs) + DestReg * 8, 8);

  auto ConstantOp = IRList.AllocateOp<IROp_Constant, OP_CONSTANT>();
  ConstantOp.first->Flags = IR::TYPE_I64;
  ConstantOp.first->Constant = Const;

  auto AddImmOp = IRList.AllocateOp<IROp_Add, OP_ADD>();
  AddImmOp.first->Args[0] = Src;
  AddImmOp.first->Args[1] = ConstantOp.second;

  StoreContext(AddImmOp.second, offsetof(X86State, gregs) + DestReg * 8, 8);

  if (1) {
    // Set SF
    auto ConstantOp = IRList.AllocateOp<IROp_Constant, OP_CONSTANT>();
    ConstantOp.first->Flags = IR::TYPE_I64;
    ConstantOp.first->Constant = (OpSize * 4) - 1;

    auto ShrOp = IRList.AllocateOp<IROp_Shr, OP_SHR>();
    ShrOp.first->Args[0] = Src;
    ShrOp.first->Args[1] = ConstantOp.second;
    SetSF(ShrOp.second);
  }

  if (1) {
    // Set ZF
    auto ZeroConstant = IRList.AllocateOp<IROp_Constant, OP_CONSTANT>();
    ZeroConstant.first->Flags = IR::TYPE_I64;
    ZeroConstant.first->Constant = 1;

    auto OneConstant = IRList.AllocateOp<IROp_Constant, OP_CONSTANT>();
    OneConstant.first->Flags = IR::TYPE_I64;
    OneConstant.first->Constant = 0;

    auto SelectOp = IRList.AllocateOp<IROp_Select, OP_SELECT>();
    SelectOp.first->Op = IROp_Select::COMP_EQ;
    SelectOp.first->Args[0] = AddImmOp.second;
    SelectOp.first->Args[1] = ZeroConstant.second;
    SelectOp.first->Args[2] = OneConstant.second;
    SelectOp.first->Args[3] = ZeroConstant.second;
    SetZF(SelectOp.second);
  }
}

void OpDispatchBuilder::ShlImmOp(Emu::X86Tables::DecodedOp Op, uint8_t const *Code) {
  uint32_t DestReg = 0;

  if (!(Op.second.Flags & X86Tables::DECODE_FLAG_MODRM))
    DecodeFailure = true;
  if (Op.second.Flags & X86Tables::DECODE_FLAG_SIB)
    DecodeFailure = true;

  uint8_t REX = 0;
  uint8_t ModRM = 0;

  uint32_t OpSize = 4;
  if (Op.second.Flags & X86Tables::DECODE_FLAG_OPSIZE) {
    OpSize = 2;
  }

  if (Op.second.Flags & X86Tables::DECODE_FLAG_REX) {
    REX = Code[0];
    ModRM = Code[2];

    if (GetModRM_Mod(ModRM) != 0b11)
      DecodeFailure = true;

    if (GetREX_W(REX)) {
      OpSize = 8;
    }
  } else {
    ModRM = Code[1];

    if (GetModRM_Mod(ModRM) != 0b11)
      DecodeFailure = true;
  }

  if (DecodeFailure)
    return;

  {
    DestReg = MapModRMToReg(GetREX_B(REX), GetModRM_RM(ModRM));

    auto LoadOp = IRList.AllocateOp<IROp_LoadContext, OP_LOADCONTEXT>();
    LoadOp.first->Size = 8;
    LoadOp.first->Offset = offsetof(X86State, gregs) + DestReg * 8;

    auto ConstantOp = IRList.AllocateOp<IROp_Constant, OP_CONSTANT>();
    ConstantOp.first->Flags = IR::TYPE_I8;
    ConstantOp.first->Constant = *(int8_t*)&Code[Op.second.Size - 1];

    auto Src = LoadOp.second;
    switch (OpSize) {
    case 1: LogMan::Msg::A("Unhandled Add size 1"); break;
    case 2: {
      auto Trunc1 = IRList.AllocateOp<IROp_Trunc_16, OP_TRUNC_16>();
      Trunc1.first->Arg = Src;
      Src = Trunc1.second;
    }
    break;
    case 4: {
      auto Trunc1 = IRList.AllocateOp<IROp_Trunc_32, OP_TRUNC_32>();
      Trunc1.first->Arg = Src;
      Src = Trunc1.second;
    }
    break;
    case 8: break;
    }

    auto ShlOp = IRList.AllocateOp<IROp_Shl, OP_SHL>();
    ShlOp.first->Args[0] = Src;
    ShlOp.first->Args[1] = ConstantOp.second;
    auto Res = ShlOp.second;

    switch (OpSize) {
    case 1: LogMan::Msg::A("Unhandled Add size 1"); break;
    case 2: {
      auto Trunc = IRList.AllocateOp<IROp_Trunc_16, OP_TRUNC_16>();
      Trunc.first->Arg = Res;

      Res = Trunc.second;
    }
    break;
    case 4: {
      auto Trunc = IRList.AllocateOp<IROp_Trunc_32, OP_TRUNC_32>();
      Trunc.first->Arg = Res;

      Res = Trunc.second;
    }
    break;
    case 8: break;
    }

    auto StoreOp = IRList.AllocateOp<IROp_StoreContext, OP_STORECONTEXT>();
    StoreOp.first->Size = 8;
    StoreOp.first->Offset = offsetof(X86State, gregs) + DestReg * 8;
    StoreOp.first->Arg = Res;

    // Calculate CF value
    if (0) {
      uint32_t ShrAmount = (OpSize * 8) - ConstantOp.first->Constant;
      auto ShrConstantOp = IRList.AllocateOp<IROp_Constant, OP_CONSTANT>();
      ShrConstantOp.first->Flags = IR::TYPE_I8;
      ShrConstantOp.first->Constant = ShrAmount;

      auto ShrOp = IRList.AllocateOp<IROp_Shl, OP_SHR>();
      ShrOp.first->Args[0] = Src;
      ShrOp.first->Args[1] = ShrConstantOp.second;
      SetCF(ShrOp.second);
    }
  }
}

void OpDispatchBuilder::XorOp(Emu::X86Tables::DecodedOp Op, uint8_t const *Code) {
  uint32_t DestReg = 0;
  uint32_t SrcReg = 0;

  if (!(Op.second.Flags & X86Tables::DECODE_FLAG_MODRM))
    DecodeFailure = true;
  if (Op.second.Flags & X86Tables::DECODE_FLAG_SIB)
    DecodeFailure = true;

  uint8_t REX = 0;
  uint8_t ModRM = 0;

  uint32_t OpSize = 4;
  if (Op.second.Flags & X86Tables::DECODE_FLAG_OPSIZE) {
    OpSize = 2;
  }

  if (Op.second.Flags & X86Tables::DECODE_FLAG_REX) {
    REX = Code[0];
    ModRM = Code[2];

    if (GetREX_W(REX)) {
      OpSize = 8;
    }

    if (GetModRM_Mod(ModRM) != 0b11)
      DecodeFailure = true;

  } else {
    ModRM = Code[1];

    if (GetModRM_Mod(ModRM) != 0b11)
      DecodeFailure = true;
  }

  if (DecodeFailure)
    return;

  DestReg = MapModRMToReg(GetREX_B(REX), GetModRM_RM(ModRM));
  SrcReg = MapModRMToReg(GetREX_R(REX), GetModRM_Reg(ModRM));

  auto Src1 = LoadContext(offsetof(X86State, gregs) + DestReg * 8, 8);
  auto Src2 = LoadContext(offsetof(X86State, gregs) + SrcReg * 8, 8);

  Src1 = Truncate(Src1, OpSize);
  Src2 = Truncate(Src2, OpSize);

  auto XorOp = IRList.AllocateOp<IROp_Xor, OP_XOR>();
  XorOp.first->Args[0] = Src1;
  XorOp.first->Args[1] = Src2;

  StoreContext(Truncate(XorOp.second, OpSize), offsetof(X86State, gregs) + DestReg * 8, 8);
}

void OpDispatchBuilder::MovOp(Emu::X86Tables::DecodedOp Op, uint8_t const *Code) {
  uint32_t DestReg = 0;
  uint32_t SrcReg = 0;

  if (!(Op.second.Flags & X86Tables::DECODE_FLAG_MODRM))
    DecodeFailure = true;
  if (Op.second.Flags & X86Tables::DECODE_FLAG_OPSIZE)
    DecodeFailure = true;
  if (Op.second.Flags & X86Tables::DECODE_FLAG_SIB)
    DecodeFailure = true;

  uint8_t ModRM = 0;

  if (Op.second.Flags & X86Tables::DECODE_FLAG_REX) {
    uint8_t REX = Code[0];
    ModRM = Code[2];
    DestReg = MapModRMToReg(GetREX_B(REX), GetModRM_RM(ModRM));
    SrcReg = MapModRMToReg(GetREX_R(REX), GetModRM_Reg(ModRM));
  }
  else {
    ModRM = Code[1];
    DestReg = MapModRMToReg(GetModRM_RM(ModRM));
    SrcReg = MapModRMToReg(GetModRM_Reg(ModRM));
  }

  if (GetModRM_Mod(ModRM) != 0b11)
    DecodeFailure = true;

  if (DecodeFailure)
    return;

  auto LoadOp = IRList.AllocateOp<IROp_LoadContext, OP_LOADCONTEXT>();
  LoadOp.first->Size = 8;
  LoadOp.first->Offset = offsetof(X86State, gregs) + SrcReg * 8;

  AlignmentType ResultOffset = LoadOp.second;

  if (!(Op.second.Flags & X86Tables::DECODE_FLAG_REX)) {
    auto Trunc_32Op = IRList.AllocateOp<IROp_Trunc_32, OP_TRUNC_32>();
    Trunc_32Op.first->Arg = LoadOp.second;
    ResultOffset = Trunc_32Op.second;
  }

  auto StoreOp = IRList.AllocateOp<IROp_StoreContext, OP_STORECONTEXT>();
  StoreOp.first->Size = 8;
  StoreOp.first->Offset = offsetof(X86State, gregs) + DestReg * 8;
  StoreOp.first->Arg = ResultOffset;
}

void OpDispatchBuilder::BTOp(Emu::X86Tables::DecodedOp Op, uint8_t const *Code) {
  uint32_t DestReg = 0;
  uint32_t SrcReg = 0;
  uint8_t RexDest = 0;
  uint8_t RexSrc = 0;
  uint8_t ModRMOffset = 1;

  if (!(Op.second.Flags & X86Tables::DECODE_FLAG_MODRM))
    DecodeFailure = true;
  if (Op.second.Flags & X86Tables::DECODE_FLAG_OPSIZE)
    DecodeFailure = true;
  if (Op.second.Flags & X86Tables::DECODE_FLAG_SIB)
    DecodeFailure = true;

  uint8_t REX = 0;
  uint8_t ModRM = 0;
  if (Op.second.Flags & X86Tables::DECODE_FLAG_REX) {
    REX = Code[0];
    ModRM = Code[3];

  } else {
    ModRM = Code[2];
  }

  if (GetModRM_Mod(ModRM) != 0b11) {
    DecodeFailure = true;
  }

  if (DecodeFailure)
    return;

  DestReg = MapModRMToReg(GetREX_B(REX), GetModRM_RM(ModRM));
  SrcReg = MapModRMToReg(GetREX_R(REX), GetModRM_Reg(ModRM));

  auto Src1 = LoadContext(offsetof(X86State, gregs) + DestReg * 8, 8);
  auto Src2 = LoadContext(offsetof(X86State, gregs) + SrcReg * 8, 8);

  auto ConstantOp = IRList.AllocateOp<IROp_Constant, OP_CONSTANT>();
  ConstantOp.first->Flags = IR::TYPE_I8;
  ConstantOp.first->Constant = 31;

  auto AndOp = IRList.AllocateOp<IROp_And, OP_AND>();
  AndOp.first->Args[0] = Src2;
  AndOp.first->Args[1] = ConstantOp.second;

  auto BitExtractOp = IRList.AllocateOp<IROp_BitExtract, OP_BITEXTRACT>();
  BitExtractOp.first->Args[0] = Src1;
  BitExtractOp.first->Args[1] = AndOp.second;

  // Result sis stored in CF
  SetCF(BitExtractOp.second);
}

void OpDispatchBuilder::JMPOp(Emu::X86Tables::DecodedOp Op, uint8_t const *Code) {
  uint32_t SrcReg = 0;

  if (!(Op.second.Flags & X86Tables::DECODE_FLAG_MODRM))
    DecodeFailure = true;
  if (Op.second.Flags & X86Tables::DECODE_FLAG_SIB)
    DecodeFailure = true;

  uint8_t REX = 0;
  uint8_t ModRM = 0;

  uint32_t OpSize = 8;

  if (Op.second.Flags & X86Tables::DECODE_FLAG_REX) {
    REX = Code[0];
    ModRM = Code[2];
    printf("Hit REX %02x %02x\n", REX, ModRM);
  } else {
    ModRM = Code[1];
  }

  if (GetModRM_Mod(ModRM) != 0b11)
    DecodeFailure = true;


  if (DecodeFailure)
    return;

  SrcReg = MapModRMToReg(GetREX_B(REX), GetModRM_RM(ModRM));

  auto LoadOp = IRList.AllocateOp<IROp_LoadContext, OP_LOADCONTEXT>();
  LoadOp.first->Size = 8;
  LoadOp.first->Offset = offsetof(X86State, gregs) + SrcReg * 8;

  auto StoreOp = IRList.AllocateOp<IROp_StoreContext, OP_STORECONTEXT>();
  StoreOp.first->Size = 8;
  StoreOp.first->Offset = offsetof(X86State, rip);
  StoreOp.first->Arg = LoadOp.second;

  printf("JMP Op Reg %d(%d %d)! %d\n", SrcReg, GetREX_B(REX), GetModRM_RM(ModRM),  OpSize);
}

void OpDispatchBuilder::LEAOp(Emu::X86Tables::DecodedOp Op, uint8_t const *Code) {
//  DISABLE_DECODE();

  bool SIB = !!(Op.second.Flags & X86Tables::DECODE_FLAG_SIB);
  uint32_t DestReg = 0;
  uint32_t SrcReg = 0;

  if (!(Op.second.Flags & X86Tables::DECODE_FLAG_MODRM))
    DecodeFailure = true;

  uint8_t REX = 0;
  uint8_t ModRM = 0;
  AlignmentType Src;

  uint32_t OpSize = 4;
  if (Op.second.Flags & X86Tables::DECODE_FLAG_OPSIZE) {
    OpSize = 2;
  }

  if (Op.second.Flags & X86Tables::DECODE_FLAG_REX) {
    REX = Code[0];
    ModRM = Code[2];

    if (GetREX_W(REX)) {
      OpSize = 8;
    }
    uint8_t Mod = GetModRM_Mod(ModRM);
    uint8_t RM = GetModRM_RM(ModRM);
    uint8_t Reg = GetModRM_Reg(ModRM);
    if (Mod != 0b11 && Reg != 0b100) {
 //     printf("XXX: SIB ModRM without Byte!!Mod: %d Src: %s Base? :%s\n",
 //         Mod,
 //         RegToString(MapModRMToReg(GetREX_R(REX), GetModRM_Reg(ModRM))).c_str(),
 //         RegToString(MapModRMToReg(GetREX_B(REX), GetModRM_RM(ModRM))).c_str()
 //         );
      if (Mod == 0b10) {
        // Register + Displacement32
        DestReg = MapModRMToReg(GetREX_R(REX), GetModRM_Reg(ModRM));
        SrcReg = MapModRMToReg(GetREX_B(REX), GetModRM_RM(ModRM));
        Src = LoadContext(offsetof(X86State, gregs) + SrcReg * 8, 8);

        uint64_t Const = *(int32_t*)&Code[Op.second.Size - 4];
        auto ConstantOp = IRList.AllocateOp<IROp_Constant, OP_CONSTANT>();

        ConstantOp.first->Flags = IR::TYPE_I64;
        ConstantOp.first->Constant = Const;

        auto AddOp = IRList.AllocateOp<IROp_Add, OP_ADD>();
        AddOp.first->Args[0] = Src;
        AddOp.first->Args[1] = ConstantOp.second;

        Src = AddOp.second;
      }
      else {
        DecodeFailure = true;
      }
    }
    else
      DecodeFailure = true;

  } else {
    ModRM = Code[1];

    DecodeFailure = true;
    if (GetModRM_Mod(ModRM) != 0b11)
      DecodeFailure = true;
  }

//    DecodeFailure = true;

  if (DecodeFailure)
    return;

  // 48 8d 97 90 01 00 00    lea    rdx,[rdi+0x190]
  StoreContext(Src, offsetof(X86State, gregs) + DestReg * 8, 8);
}

void OpDispatchBuilder::SyscallOp(Emu::X86Tables::DecodedOp Op, uint8_t const *Code) {
  std::array<AlignmentType, 6> ArgOffsets;
  std::array<uint64_t, 7> GPRIndexes = {
    REG_RAX,
    REG_RDI,
    REG_RSI,
    REG_RDX,
    REG_R10,
    REG_R8,
    REG_R9,
  };
  for (int i = 0; i < IROp_Syscall::MAX_ARGS; ++i) {
    auto Arg = IRList.AllocateOp<IROp_LoadContext, OP_LOADCONTEXT>();
    Arg.first->Size = 8;
    Arg.first->Offset = offsetof(X86State, gregs) + GPRIndexes[i] * 8;
    ArgOffsets[i] = Arg.second;
  }

  auto SyscallOp = IRList.AllocateOp<IROp_Syscall, OP_SYSCALL>();
  for (int i = 0; i < IROp_Syscall::MAX_ARGS; ++i) {
    SyscallOp.first->Arguments[i] = ArgOffsets[i];
  }

  auto StoreOp = IRList.AllocateOp<IROp_StoreContext, OP_STORECONTEXT>();
  StoreOp.first->Size = 8;
  StoreOp.first->Offset = offsetof(X86State, gregs[REG_RAX]);
  StoreOp.first->Arg = SyscallOp.second;
}

void OpDispatchBuilder::UnknownOp(Emu::X86Tables::DecodedOp Op, uint8_t const *Code) {
  std::ostringstream str;
  str << "Unknown Op: " << Op.first->Name << " 0x";
  for (int i = 0; i < 4; ++i) {
    str << " " << std::setw(2) << std::setfill('0') << std::hex << (uint32_t)Code[i];
  }
  LogMan::Msg::A(str.str().c_str());
}

void OpDispatchBuilder::NoOp(Emu::X86Tables::DecodedOp Op, uint8_t const *Code) {
}

void OpDispatchBuilder::SetCF(AlignmentType Value) {
  auto LoadOp = IRList.AllocateOp<IROp_LoadContext, OP_LOADCONTEXT>();
  LoadOp.first->Size = 8;
  LoadOp.first->Offset = offsetof(X86State, rflags);

  // Value put in in to the CF is in bit 0 of Value
  auto ConstantOp = IRList.AllocateOp<IROp_Constant, OP_CONSTANT>();

  ConstantOp.first->Flags = IR::TYPE_I64;
  ConstantOp.first->Constant = 1;

  auto AndOp = IRList.AllocateOp<IROp_And, OP_AND>();
  AndOp.first->Args[0] = Value;
  AndOp.first->Args[1] = ConstantOp.second;

  auto NandOp = IRList.AllocateOp<IROp_Nand, OP_NAND>();
  NandOp.first->Args[0] = LoadOp.second;
  NandOp.first->Args[1] = ConstantOp.second;

  auto OrOp = IRList.AllocateOp<IROp_Or, OP_OR>();
  OrOp.first->Args[0] = AndOp.second;
  OrOp.first->Args[1] = NandOp.second;

  auto StoreOp = IRList.AllocateOp<IROp_StoreContext, OP_STORECONTEXT>();
  StoreOp.first->Size = 8;
  StoreOp.first->Offset = offsetof(X86State, rflags);
  StoreOp.first->Arg = OrOp.second;
}

void OpDispatchBuilder::SetZF(AlignmentType Value) {
  constexpr uint32_t BIT_LOCATION = 6;
  auto LoadOp = IRList.AllocateOp<IROp_LoadContext, OP_LOADCONTEXT>();
  LoadOp.first->Size = 8;
  LoadOp.first->Offset = offsetof(X86State, rflags);

  // Value put in in to the ZF is in bit 0 of Value
  auto ConstantOp = IRList.AllocateOp<IROp_Constant, OP_CONSTANT>();
  ConstantOp.first->Flags = IR::TYPE_I64;
  ConstantOp.first->Constant = 1;

  auto ZFBitConstant = IRList.AllocateOp<IROp_Constant, OP_CONSTANT>();
  ZFBitConstant.first->Flags = IR::TYPE_I64;
  ZFBitConstant.first->Constant = (1 << BIT_LOCATION);

  auto ShiftConstant = IRList.AllocateOp<IROp_Constant, OP_CONSTANT>();
  ShiftConstant.first->Flags = IR::TYPE_I64;
  ShiftConstant.first->Constant = BIT_LOCATION;

  // Clear the rest of the bits
  auto AndOp = IRList.AllocateOp<IROp_And, OP_AND>();
  AndOp.first->Args[0] = Value;
  AndOp.first->Args[1] = ConstantOp.second;

  // Shift in to the correct position
  auto ShiftOp = IRList.AllocateOp<IROp_Shl, OP_SHL>();
  ShiftOp.first->Args[0] = AndOp.second;
  ShiftOp.first->Args[1] = ShiftConstant.second;

  // Clear the location in the FLAGS
  auto NandOp = IRList.AllocateOp<IROp_Nand, OP_NAND>();
  NandOp.first->Args[0] = LoadOp.second;
  NandOp.first->Args[1] = ZFBitConstant.second;

  // Or the values together
  auto OrOp = IRList.AllocateOp<IROp_Or, OP_OR>();
  OrOp.first->Args[0] = ShiftOp.second;
  OrOp.first->Args[1] = NandOp.second;

  auto StoreOp = IRList.AllocateOp<IROp_StoreContext, OP_STORECONTEXT>();
  StoreOp.first->Size = 8;
  StoreOp.first->Offset = offsetof(X86State, rflags);
  StoreOp.first->Arg = OrOp.second;
}

void OpDispatchBuilder::SetSF(AlignmentType Value) {
  constexpr uint32_t BIT_LOCATION = 7;
  auto LoadOp = IRList.AllocateOp<IROp_LoadContext, OP_LOADCONTEXT>();
  LoadOp.first->Size = 8;
  LoadOp.first->Offset = offsetof(X86State, rflags);

  // Value put in in to the SF is in bit 0 of Value
  auto ConstantOp = IRList.AllocateOp<IROp_Constant, OP_CONSTANT>();
  ConstantOp.first->Flags = IR::TYPE_I64;
  ConstantOp.first->Constant = 1;

  auto SFBitConstant = IRList.AllocateOp<IROp_Constant, OP_CONSTANT>();
  SFBitConstant.first->Flags = IR::TYPE_I64;
  SFBitConstant.first->Constant = (1 << BIT_LOCATION);

  auto ShiftConstant = IRList.AllocateOp<IROp_Constant, OP_CONSTANT>();
  ShiftConstant.first->Flags = IR::TYPE_I64;
  ShiftConstant.first->Constant = BIT_LOCATION;

  // Clear the rest of the bits
  auto AndOp = IRList.AllocateOp<IROp_And, OP_AND>();
  AndOp.first->Args[0] = Value;
  AndOp.first->Args[1] = ConstantOp.second;

  // Shift in to the correct position
  auto ShiftOp = IRList.AllocateOp<IROp_Shl, OP_SHL>();
  ShiftOp.first->Args[0] = AndOp.second;
  ShiftOp.first->Args[1] = ShiftConstant.second;

  // Clear the location in the FLAGS
  auto NandOp = IRList.AllocateOp<IROp_Nand, OP_NAND>();
  NandOp.first->Args[0] = LoadOp.second;
  NandOp.first->Args[1] = SFBitConstant.second;

  // Or the values together
  auto OrOp = IRList.AllocateOp<IROp_Or, OP_OR>();
  OrOp.first->Args[0] = ShiftOp.second;
  OrOp.first->Args[1] = NandOp.second;

  auto StoreOp = IRList.AllocateOp<IROp_StoreContext, OP_STORECONTEXT>();
  StoreOp.first->Size = 8;
  StoreOp.first->Offset = offsetof(X86State, rflags);
  StoreOp.first->Arg = OrOp.second;
}

void OpDispatchBuilder::SetOF(AlignmentType Value) {
  constexpr uint32_t BIT_LOCATION = 11;
  auto LoadOp = IRList.AllocateOp<IROp_LoadContext, OP_LOADCONTEXT>();
  LoadOp.first->Size = 8;
  LoadOp.first->Offset = offsetof(X86State, rflags);

  // Value put in in to the OF is in bit 0 of Value
  auto ConstantOp = IRList.AllocateOp<IROp_Constant, OP_CONSTANT>();
  ConstantOp.first->Flags = IR::TYPE_I64;
  ConstantOp.first->Constant = 1;

  auto OFBitConstant = IRList.AllocateOp<IROp_Constant, OP_CONSTANT>();
  OFBitConstant.first->Flags = IR::TYPE_I64;
  OFBitConstant.first->Constant = (1 << BIT_LOCATION);

  auto ShiftConstant = IRList.AllocateOp<IROp_Constant, OP_CONSTANT>();
  ShiftConstant.first->Flags = IR::TYPE_I64;
  ShiftConstant.first->Constant = BIT_LOCATION;

  // Clear the rest of the bits
  auto AndOp = IRList.AllocateOp<IROp_And, OP_AND>();
  AndOp.first->Args[0] = Value;
  AndOp.first->Args[1] = ConstantOp.second;

  // Shift in to the correct position
  auto ShiftOp = IRList.AllocateOp<IROp_Shl, OP_SHL>();
  ShiftOp.first->Args[0] = AndOp.second;
  ShiftOp.first->Args[1] = ShiftConstant.second;

  // Clear the location in the FLAGS
  auto NandOp = IRList.AllocateOp<IROp_Nand, OP_NAND>();
  NandOp.first->Args[0] = LoadOp.second;
  NandOp.first->Args[1] = OFBitConstant.second;

  // Or the values together
  auto OrOp = IRList.AllocateOp<IROp_Or, OP_OR>();
  OrOp.first->Args[0] = ShiftOp.second;
  OrOp.first->Args[1] = NandOp.second;

  auto StoreOp = IRList.AllocateOp<IROp_StoreContext, OP_STORECONTEXT>();
  StoreOp.first->Size = 8;
  StoreOp.first->Offset = offsetof(X86State, rflags);
  StoreOp.first->Arg = OrOp.second;
}

AlignmentType OpDispatchBuilder::GetFlagBit(uint32_t bit, bool negate) {
  auto LoadOp = IRList.AllocateOp<IROp_LoadContext, OP_LOADCONTEXT>();
  LoadOp.first->Size = 8;
  LoadOp.first->Offset = offsetof(X86State, rflags);

  auto ConstantOp = IRList.AllocateOp<IROp_Constant, OP_CONSTANT>();
  ConstantOp.first->Flags = IR::TYPE_I64;
  ConstantOp.first->Constant = 1;

  auto BitConstant = IRList.AllocateOp<IROp_Constant, OP_CONSTANT>();
  BitConstant.first->Flags = IR::TYPE_I64;
  BitConstant.first->Constant = (1 << bit);

  auto ShiftConstant = IRList.AllocateOp<IROp_Constant, OP_CONSTANT>();
  ShiftConstant.first->Flags = IR::TYPE_I64;
  ShiftConstant.first->Constant = bit;

  auto AndOp = IRList.AllocateOp<IROp_And, OP_AND>();
  AndOp.first->Args[0] = LoadOp.second;
  AndOp.first->Args[1] = BitConstant.second;

  // Shift in to the correct position
  auto ShiftOp = IRList.AllocateOp<IROp_Shr, OP_SHR>();
  ShiftOp.first->Args[0] = AndOp.second;
  ShiftOp.first->Args[1] = ShiftConstant.second;

  if (negate) {
    auto XorOp = IRList.AllocateOp<IROp_Xor, OP_XOR>();
    XorOp.first->Args[0] = XorOp.second;
    XorOp.first->Args[1] = ConstantOp.second;
    return XorOp.second;
  }
  else {
    return ShiftOp.second;
  }
}

AlignmentType OpDispatchBuilder::LoadContext(uint64_t Offset, uint64_t Size) {
  auto LoadOp = IRList.AllocateOp<IROp_LoadContext, OP_LOADCONTEXT>();
  LoadOp.first->Size = Size;
  LoadOp.first->Offset = Offset;
  return LoadOp.second;
}

void OpDispatchBuilder::StoreContext(AlignmentType Value, uint64_t Offset, uint64_t Size) {
  auto StoreOp = IRList.AllocateOp<IROp_StoreContext, OP_STORECONTEXT>();
  StoreOp.first->Size = Size;
  StoreOp.first->Offset = Offset;
  StoreOp.first->Arg = Value;
}

AlignmentType OpDispatchBuilder::Truncate(AlignmentType Value, uint64_t Size) {
  switch (Size) {
  case 8: return Value;
  case 4: {
    auto Trunc = IRList.AllocateOp<IROp_Trunc_32, OP_TRUNC_32>();
    Trunc.first->Arg = Value;
    return Trunc.second;
  }
  break;
  case 2: {
    auto Trunc = IRList.AllocateOp<IROp_Trunc_16, OP_TRUNC_16>();
    Trunc.first->Arg = Value;
    return Trunc.second;
  }
  break;
  default:
    LogMan::Msg::A("Unhandled truncate size\n");
  break;
  }
  return ~0;
}


OpDispatchBuilder::OpDispatchBuilder(CPUCore *CPU)
  : cpu {CPU} {
}

void InstallOpcodeHandlers() {
  const std::vector<std::tuple<uint8_t, uint8_t, X86Tables::OpDispatchPtr>> BaseOpTable = {
    // Instructions
    {0x00, 1, &OpDispatchBuilder::UnknownOp},
    {0x01, 1, &OpDispatchBuilder::AddOp},
    {0x02, 1, &OpDispatchBuilder::UnknownOp},
    {0x03, 1, &OpDispatchBuilder::AddOp},
    {0x04, 1, &OpDispatchBuilder::UnknownOp},
    {0x05, 1, &OpDispatchBuilder::AddImmOp},
    {0x31, 1, &OpDispatchBuilder::XorOp},
    {0x39, 1, &OpDispatchBuilder::CMPOp},
    {0x70, 1, &OpDispatchBuilder::JccOp<OpDispatchBuilder::CC_OF>},
    {0x71, 1, &OpDispatchBuilder::JccOp<OpDispatchBuilder::CC_NOF>},
    {0x72, 1, &OpDispatchBuilder::JccOp<OpDispatchBuilder::CC_C>},
    {0x73, 1, &OpDispatchBuilder::JccOp<OpDispatchBuilder::CC_NC>},
    {0x74, 1, &OpDispatchBuilder::JccOp<OpDispatchBuilder::CC_Z>},
    {0x75, 1, &OpDispatchBuilder::JccOp<OpDispatchBuilder::CC_NZ>},
    {0x76, 1, &OpDispatchBuilder::JccOp<OpDispatchBuilder::CC_BE>},
    {0x77, 1, &OpDispatchBuilder::JccOp<OpDispatchBuilder::CC_NBE>},
    {0x78, 1, &OpDispatchBuilder::JccOp<OpDispatchBuilder::CC_S>},
    {0x79, 1, &OpDispatchBuilder::JccOp<OpDispatchBuilder::CC_NS>},
    {0x7A, 1, &OpDispatchBuilder::JccOp<OpDispatchBuilder::CC_P>},
    {0x7B, 1, &OpDispatchBuilder::JccOp<OpDispatchBuilder::CC_NP>},
    {0x7C, 1, &OpDispatchBuilder::JccOp<OpDispatchBuilder::CC_L>},
    {0x7D, 1, &OpDispatchBuilder::JccOp<OpDispatchBuilder::CC_NL>},
    {0x7E, 1, &OpDispatchBuilder::JccOp<OpDispatchBuilder::CC_LE>},
    {0x7F, 1, &OpDispatchBuilder::JccOp<OpDispatchBuilder::CC_NLE>},
    {0x89, 1, &OpDispatchBuilder::MovOp},
    {0x8D, 1, &OpDispatchBuilder::LEAOp},
    {0x90, 1, &OpDispatchBuilder::NoOp},
    {0xC3, 1, &OpDispatchBuilder::RETOp},
  };

  const std::vector<std::tuple<uint8_t, uint8_t, X86Tables::OpDispatchPtr>> TwoByteOpTable = {
    // Instructions
    {0x05, 1, &OpDispatchBuilder::SyscallOp},
    {0x1F, 1, &OpDispatchBuilder::NoOp},
    {0xA3, 1, &OpDispatchBuilder::BTOp},
  };

  const std::vector<std::tuple<uint16_t, uint8_t, X86Tables::OpDispatchPtr>> ModRMOpTable = {
    {0x8300, 1, &OpDispatchBuilder::AddImmModRMOp},
    {0xC104, 1, &OpDispatchBuilder::ShlImmOp},
    {0xFF04, 1, &OpDispatchBuilder::JMPOp},
  };

  auto InstallToTable = [](auto& FinalTable, auto& LocalTable) {
    for (auto Op : LocalTable) {
      auto OpNum = std::get<0>(Op);
      auto Dispatcher = std::get<2>(Op);
      for (uint8_t i = 0; i < std::get<1>(Op); ++i) {
        LogMan::Throw::A(FinalTable[OpNum + i].OpcodeDispatcher == 0, "Duplicate Entry");
        FinalTable[OpNum + i].OpcodeDispatcher = Dispatcher;
      }
    }
  };

  InstallToTable(Emu::X86Tables::BaseOps, BaseOpTable);
  InstallToTable(Emu::X86Tables::SecondBaseOps, TwoByteOpTable);
  InstallToTable(Emu::X86Tables::ModRMOps, ModRMOpTable);

}
}
