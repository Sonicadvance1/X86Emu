#pragma once
#include <array>
#include <cstdint>

namespace Emu::IR {

using AlignmentType = uint32_t;
enum IROps : unsigned {
  OP_CONSTANT     = 0,
	OP_LOADCONTEXT,
	OP_STORECONTEXT,
  OP_BEGINBLOCK,
  OP_ENDBLOCK,

  // Instructions
  OP_ADD,
  OP_XOR,
  OP_TRUNC_32,
  OP_SYSCALL,

	OP_LASTOP,
};

struct IROp_Header {
  void* Data[0];
  IROps Op : 8;

  template<typename T>
  T const* C() const { return reinterpret_cast<T const*>(Data); }
} __attribute__((packed));

struct IROp_Constant {
  enum TYPE_FLAGS {
    TYPE_I1      = (1 << 0),
    TYPE_I8      = (1 << 1),
    TYPE_I16     = (1 << 2),
    TYPE_I32     = (1 << 3),
    TYPE_I64     = (1 << 4),
    TYPE_SIGNED  = (1 << 5),
  };
  IROp_Header Header;
  uint8_t Flags;
  uint64_t Constant;
};

struct IROp_LoadContext {
  IROp_Header Header;
	uint8_t Size;
	uint32_t Offset;
};

struct IROp_StoreContext {
  IROp_Header Header;
	uint8_t Size;
	uint32_t Offset;
  AlignmentType Arg;
};


struct IROp_BeginBlock {
  IROp_Header Header;
};

struct IROp_EndBlock {
  IROp_Header Header;
  uint64_t RIPIncrement;
};

// Instruction structs
struct IROp_Add {
  IROp_Header Header;
  AlignmentType Args[2];
};
using IROp_Xor = IROp_Add;

struct IROp_Trunc_32 {
  IROp_Header Header;
  AlignmentType Arg;
};

struct IROp_Syscall {
  IROp_Header Header;
  // Maximum number of arguments including Syscall ID
  static constexpr std::size_t MAX_ARGS = 7;
  AlignmentType Arguments[MAX_ARGS];
};

constexpr std::array<size_t, OP_LASTOP + 1> IRSizes = {
	sizeof(IROp_Constant),
	sizeof(IROp_LoadContext),
	sizeof(IROp_StoreContext),
  sizeof(IROp_BeginBlock), // BeginBlock
  sizeof(IROp_EndBlock), // EndBlock

  // Instructions
  sizeof(IROp_Add),
  sizeof(IROp_Xor),
  sizeof(IROp_Trunc_32),
	sizeof(IROp_Syscall),
	-1ULL,
};

// Try to make sure our array mape directly to the IROps enum
static_assert(IRSizes[OP_LASTOP] == -1ULL);

std::string& GetName(IROps Op);
static size_t GetSize(IROps Op) { return IRSizes[Op]; }
}
