#pragma once
#include "IntrusiveIRList.h"
#include "X86Tables.h"
#include <unordered_map>

namespace Emu {
class CPUCore;
}
namespace Emu::IR {

class OpDispatchBuilder final {
public:
  OpDispatchBuilder(CPUCore *CPU);

  void BeginBlock();
  void EndBlock(uint64_t RIPIncrement);

  // Op handlers
  void AddOp(Emu::X86Tables::DecodedOp Op, uint8_t const *Code);
  void AddImmOp(Emu::X86Tables::DecodedOp Op, uint8_t const *Code);
  void AddImmModRMOp(Emu::X86Tables::DecodedOp Op, uint8_t const *Code);
  void ShlImmOp(Emu::X86Tables::DecodedOp Op, uint8_t const *Code);
  void XorOp(Emu::X86Tables::DecodedOp Op, uint8_t const *Code);
  void MovOp(Emu::X86Tables::DecodedOp Op, uint8_t const *Code);
  void BTOp(Emu::X86Tables::DecodedOp Op, uint8_t const *Code);
  void JMPOp(Emu::X86Tables::DecodedOp Op, uint8_t const *Code);
  void LEAOp(Emu::X86Tables::DecodedOp Op, uint8_t const *Code);
  void CMPOp(Emu::X86Tables::DecodedOp Op, uint8_t const *Code);

  void SyscallOp(Emu::X86Tables::DecodedOp Op, uint8_t const *Code);
  void UnknownOp(Emu::X86Tables::DecodedOp Op, uint8_t const *Code);
  void NoOp(Emu::X86Tables::DecodedOp Op, uint8_t const *Code);

  // Jump types
  enum JumpType {
    CC_OF,
    CC_NOF,
    CC_C,
    CC_NC,
    CC_Z,
    CC_NZ,
    CC_BE,
    CC_NBE,
    CC_S,
    CC_NS,
    CC_P,
    CC_NP,
    CC_L,
    CC_NL,
    CC_LE,
    CC_NLE,
  };
  template<uint32_t Type>
  void JccOp(Emu::X86Tables::DecodedOp Op, uint8_t const *Code);
  void RETOp(Emu::X86Tables::DecodedOp Op, uint8_t const *Code);

  Emu::IR::IntrusiveIRList const &GetWorkingIR() { return IRList; }
  void ResetWorkingList() { RIPLocations.clear(); IRList.Reset(); DecodeFailure = false; }
  bool HadDecodeFailure() { return DecodeFailure; }
  void AddRIPMarker(uint64_t RIP) {
    auto Marker = IRList.AllocateOp<IROp_RIPMarker, OP_RIP_MARKER>();
    Marker.first->RIP = RIP;
    RIPLocations[RIP] = Marker.second;
  }

private:
  AlignmentType LoadContext(uint64_t Offset, uint64_t Size);
  void StoreContext(AlignmentType Value, uint64_t Offset, uint64_t Size);

  AlignmentType Truncate(AlignmentType Value, uint64_t Size);
  AlignmentType GetFlagBit(uint32_t bit, bool negate);
  void SetCF(AlignmentType Value);
  void SetZF(AlignmentType Value);
  void SetSF(AlignmentType Value);
  void SetOF(AlignmentType Value);
  Emu::IR::IntrusiveIRList IRList{8 * 1024 * 1024};
  std::unordered_map<uint64_t, uint32_t> RIPLocations;

  CPUCore *cpu;
  bool DecodeFailure{false};
};

void InstallOpcodeHandlers();
}
