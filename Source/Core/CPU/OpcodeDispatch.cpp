#include "Core/CPU/CPUCore.h"
#include "Core/CPU/X86Tables.h"
#include "LogManager.h"
#include "OpcodeDispatch.h"
#include <cstddef>
#include <vector>

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
static uint8_t GetREX_R(uint8_t rex) {
  return (rex & 0b0100) >> 2;
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
  auto LoadOp = IRList.AllocateOp<IROp_LoadContext, OP_LOADCONTEXT>();
  LoadOp.first->Size = 8;
  LoadOp.first->Offset = offsetof(X86State, gregs[REG_RAX]);

  auto ConstantOp = IRList.AllocateOp<IROp_Constant, OP_CONSTANT>();

  if (Op.second.Flags & X86Tables::DECODE_FLAG_REX) {
    ConstantOp.first->Flags = IROp_Constant::TYPE_I64;
    ConstantOp.first->Constant = *(int32_t*)&Code[Op.second.Size - 4];
  }
  else if (Op.second.Flags & X86Tables::DECODE_FLAG_OPSIZE) {
    ConstantOp.first->Flags = IROp_Constant::TYPE_I16;
    ConstantOp.first->Constant = *(int16_t*)&Code[Op.second.Size - 2];
  }
  else {
    ConstantOp.first->Flags = IROp_Constant::TYPE_I32;
    ConstantOp.first->Constant = *(int32_t*)&Code[Op.second.Size - 4];
  }

  auto AddOp = IRList.AllocateOp<IROp_Add, OP_ADD>();
  AddOp.first->Args[0] = LoadOp.second;
  AddOp.first->Args[1] = ConstantOp.second;

  auto StoreOp = IRList.AllocateOp<IROp_StoreContext, OP_STORECONTEXT>();
  StoreOp.first->Size = 8;
  StoreOp.first->Offset = offsetof(X86State, gregs[REG_RAX]);
  StoreOp.first->Arg = AddOp.second;
}

void OpDispatchBuilder::XorOp(Emu::X86Tables::DecodedOp Op, uint8_t const *Code) {
  uint32_t DestReg = 0;
  uint32_t SrcReg = 0;
  uint8_t RexDest = 0;
  uint8_t RexSrc = 0;
  uint8_t ModRMOffset = 1;
  DecodeFailure = true;

  if (!(Op.second.Flags & X86Tables::DECODE_FLAG_MODRM))
    DecodeFailure = true;
  if (Op.second.Flags & X86Tables::DECODE_FLAG_OPSIZE)
    DecodeFailure = true;
  if (Op.second.Flags & X86Tables::DECODE_FLAG_SIB)
    DecodeFailure = true;

  if (Op.second.Flags & X86Tables::DECODE_FLAG_REX) {
    uint8_t REX = Code[0];
    uint8_t ModRM = Code[2];

    if (GetModRM_Mod(ModRM) != 0b11)
      DecodeFailure = true;

    DestReg = MapModRMToReg(GetREX_B(REX), GetModRM_RM(ModRM));
    SrcReg = MapModRMToReg(GetREX_R(REX), GetModRM_Reg(ModRM));
  } else {
    uint8_t ModRM = Code[1];

    if (GetModRM_Mod(ModRM) != 0b11)
      DecodeFailure = true;

    DestReg = MapModRMToReg(GetModRM_RM(ModRM));
    SrcReg = MapModRMToReg(GetModRM_Reg(ModRM));
  }

  if (DecodeFailure)
    return;

  auto LoadOp = IRList.AllocateOp<IROp_LoadContext, OP_LOADCONTEXT>();
  LoadOp.first->Size = 8;
  LoadOp.first->Offset = offsetof(X86State, gregs) + SrcReg * 8;

  auto XorOp = IRList.AllocateOp<IROp_Xor, OP_XOR>();
  XorOp.first->Args[0] = LoadOp.second;
  XorOp.first->Args[1] = LoadOp.second;

  auto StoreOp = IRList.AllocateOp<IROp_StoreContext, OP_STORECONTEXT>();
  StoreOp.first->Size = 8;
  StoreOp.first->Offset = offsetof(X86State, gregs) + DestReg * 8;
  StoreOp.first->Arg = XorOp.second;
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

OpDispatchBuilder::OpDispatchBuilder(CPUCore *CPU)
  : cpu {CPU} {
}

void InstallOpcodeHandlers() {
  const std::vector<std::tuple<uint8_t, uint8_t, X86Tables::OpDispatchPtr>> BaseOpTable = {
    // Instructions
    {0x05, 1, &OpDispatchBuilder::AddOp},
    {0x31, 1, &OpDispatchBuilder::XorOp},
    {0x89, 1, &OpDispatchBuilder::MovOp},
  };

  const std::vector<std::tuple<uint8_t, uint8_t, X86Tables::OpDispatchPtr>> TwoByteOpTable = {
    // Instructions
    {0x05, 1, &OpDispatchBuilder::SyscallOp},
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
}
}
