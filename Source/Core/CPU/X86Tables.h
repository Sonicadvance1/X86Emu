#pragma once

#include <array>

namespace Emu {
namespace IR {
  struct OpDispatchBuilder;
}
namespace X86Tables {

enum InstType {
  TYPE_UNKNOWN,
  TYPE_LEGACY_PREFIX,
  TYPE_PREFIX,
  TYPE_REX_PREFIX,
  TYPE_MODRM_TABLE_PREFIX,
  TYPE_INST,
};
enum InstFlags {
  FLAGS_NONE                = 0,
  FLAGS_HAS_MODRM           = (1 << 0),
  FLAGS_DISPLACE_SIZE_MUL_2 = (1 << 1),
  FLAGS_DISPLACE_SIZE_DIV_2 = (1 << 2),
  FLAGS_REX_IN_BYTE         = (1 << 3),
  FLAGS_BLOCK_END           = (1 << 4),
};

enum DecodeFlags {
  DECODE_FLAG_NONE   = 0,
  DECODE_FLAG_REX    = (1 << 0),
  DECODE_FLAG_OPSIZE = (1 << 1),
  DECODE_FLAG_ADSIZE = (1 << 2),
  DECODE_FLAG_MODRM  = (1 << 3),
  DECODE_FLAG_SIB    = (1 << 4),

};
struct X86InstDecodeFlags {
  uint8_t Size        : 4;
  uint8_t PrefixBytes : 4;
  uint8_t Flags; ///< Must be larger that DecodeFlags enum
};

struct X86InstInfo;
using DecodedOp = std::pair<X86InstInfo*, X86InstDecodeFlags>;
using OpDispatchPtr = void (IR::OpDispatchBuilder::*)(Emu::X86Tables::DecodedOp, uint8_t const *Code);

struct X86InstInfo {
  char const *Name;
  InstType Type;
  uint8_t Flags; ///< Must be larger than InstFlags enum
  uint8_t MoreBytes;
  OpDispatchPtr OpcodeDispatcher;
};
extern std::array<X86InstInfo, 255> BaseOps;
extern std::array<X86InstInfo, 255> SecondBaseOps;
extern std::array<X86InstInfo, (1 << 16) - 1> ModRMOps;

void InitializeInfoTables();
DecodedOp GetInstInfo(uint8_t const *Inst);
}
}
