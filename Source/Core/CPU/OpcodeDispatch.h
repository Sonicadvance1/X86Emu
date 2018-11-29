#pragma once
#include "IntrusiveIRList.h"
#include "X86Tables.h"

namespace Emu {
class CPUCore;
}
namespace Emu::IR {

class OpDispatchBuilder {
public:
  OpDispatchBuilder(CPUCore *CPU);

  void BeginBlock();
  void EndBlock(uint64_t RIPIncrement);
  // Op handlers
  void AddOp(Emu::X86Tables::DecodedOp Op, uint8_t const *Code);
  void XorOp(Emu::X86Tables::DecodedOp Op, uint8_t const *Code);
  void MovOp(Emu::X86Tables::DecodedOp Op, uint8_t const *Code);
  void SyscallOp(Emu::X86Tables::DecodedOp Op, uint8_t const *Code);

  Emu::IR::IntrusiveIRList const &GetWorkingIR() { return IRList; }
  void ResetWorkingList() { IRList.Reset(); DecodeFailure = false; }
  bool HadDecodeFailure() { return DecodeFailure; }

private:
  Emu::IR::IntrusiveIRList IRList{8 * 1024 * 1024};

  CPUCore *cpu;
  bool DecodeFailure{false};
};

void InstallOpcodeHandlers();
}
