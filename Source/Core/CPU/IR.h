#pragma once
#include <array>
#include <cstdint>

namespace Emu::IR {

using AlignmentType = uint32_t;
class IntrusiveIRList;
enum IROps : unsigned {
  OP_CONSTANT     = 0,
	OP_LOADCONTEXT,
	OP_STORECONTEXT,

  // Function managment
  OP_BEGINFUNCTION,
  OP_ENDFUNCTION,
  OP_GETARGUMENT,
  OP_ALLOCATE_CONTEXT,

  // Block management
  OP_BEGINBLOCK,
  OP_ENDBLOCK,

  // Branching
  OP_JUMP,
  OP_COND_JUMP,
  OP_CALL,
  OP_EXTERN_CALL,
  OP_SYSCALL,
  OP_RETURN,

  // Instructions
  OP_ADD,
  OP_SUB,
  OP_OR,
  OP_XOR,
  OP_SHL,
  OP_SHR,
  OP_AND,
  OP_NAND,
  OP_BITEXTRACT,
  OP_SELECT,
  OP_TRUNC_32,
  OP_TRUNC_16,

  // Memory
  OP_LOAD_MEM,

  // Misc
  OP_JUMP_TGT,
  OP_RIP_MARKER,

	OP_LASTOP,
};

struct IROp_Header {
  void* Data[0];
  IROps Op : 8;

  template<typename T>
  T const* C() const { return reinterpret_cast<T const*>(Data); }
} __attribute__((packed));

enum TYPE_FLAGS {
  TYPE_I1      = (1 << 0),
  TYPE_I8      = (1 << 1),
  TYPE_I16     = (1 << 2),
  TYPE_I32     = (1 << 3),
  TYPE_I64     = (1 << 4),
  TYPE_SIGNED  = (1 << 5),
};

struct IROp_Constant {
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

struct IROp_BeginFunction {
  IROp_Header Header;
  uint8_t Arguments;
  bool HasReturn;
};

struct IROp_GetArgument{
  IROp_Header Header;
	uint8_t Argument;
  AlignmentType Arg;
};

struct IROp_AllocateContext{
  IROp_Header Header;
	uint64_t Size;
  AlignmentType Arg;
};

struct IROp_Empty {
  IROp_Header Header;
};

using IROp_EndFunction = IROp_Empty;
using IROp_BeginBlock = IROp_Empty;
using IROp_JmpTarget = IROp_Empty;

struct IROp_EndBlock {
  IROp_Header Header;
  uint64_t RIPIncrement;
};

// Instruction structs
struct IROp_MonoOp {
  IROp_Header Header;
  AlignmentType Arg;
};

struct IROp_BiOp {
  IROp_Header Header;
  AlignmentType Args[2];
};

struct IROp_TriOp {
  IROp_Header Header;
  AlignmentType Args[3];
};

// MonoOps
using IROp_Return = IROp_MonoOp;
using IROp_Trunc_32 = IROp_MonoOp;
using IROp_Trunc_16 = IROp_MonoOp;

// BiOps
using IROp_Add  = IROp_BiOp;
using IROp_Sub  = IROp_BiOp;
using IROp_Or   = IROp_BiOp;
using IROp_Xor  = IROp_BiOp;
using IROp_Shl  = IROp_BiOp;
using IROp_Shr  = IROp_BiOp;
using IROp_And  = IROp_BiOp;
using IROp_Nand = IROp_BiOp;
using IROp_BitExtract = IROp_BiOp;

struct IROp_Select {
  enum ComparisonOp {
    COMP_EQ,
    COMP_NEQ,
  };
  IROp_Header Header;
  ComparisonOp Op;
  AlignmentType Args[4];
};

struct IROp_LoadMem {
  IROp_Header Header;
  uint8_t Size;
  AlignmentType Arg[2];
};

struct IROp_Jump {
  IROp_Header Header;
  AlignmentType Target;
};

struct IROp_CondJump {
  IROp_Header Header;
  AlignmentType Cond;
  AlignmentType Target;
  uint64_t RIPTarget;
};

struct IROp_Call {
  IROp_Header Header;
  AlignmentType Target;
};

struct IROp_ExternCall {
  IROp_Header Header;
  AlignmentType Target;
};

struct IROp_Syscall {
  IROp_Header Header;
  // Maximum number of arguments including Syscall ID
  static constexpr std::size_t MAX_ARGS = 7;
  AlignmentType Arguments[MAX_ARGS];
};

struct IROp_RIPMarker {
  IROp_Header Header;
  uint64_t RIP;
};

constexpr std::array<size_t, OP_LASTOP + 1> IRSizes = {
	sizeof(IROp_Constant),
	sizeof(IROp_LoadContext),
	sizeof(IROp_StoreContext),

  // Function management
  sizeof(IROp_BeginFunction),
  sizeof(IROp_EndFunction),
  sizeof(IROp_GetArgument),
  sizeof(IROp_AllocateContext),

  // Block Management
  sizeof(IROp_BeginBlock), // BeginBlock
  sizeof(IROp_EndBlock), // EndBlock

  // Branching
  sizeof(IROp_Jump),
  sizeof(IROp_CondJump),
  sizeof(IROp_Call),
  sizeof(IROp_ExternCall),
	sizeof(IROp_Syscall),
	sizeof(IROp_Return),

  // Instructions
  sizeof(IROp_Add),
  sizeof(IROp_Sub),
  sizeof(IROp_Or),
  sizeof(IROp_Xor),
  sizeof(IROp_Shl),
  sizeof(IROp_Shr),
  sizeof(IROp_And),
  sizeof(IROp_Nand),
  sizeof(IROp_BitExtract),
  sizeof(IROp_Select),
  sizeof(IROp_Trunc_32),
  sizeof(IROp_Trunc_16),

  // Memory
  sizeof(IROp_LoadMem),

  // Misc
  sizeof(IROp_JmpTarget),
	sizeof(IROp_RIPMarker),
	-1ULL,
};

// Try to make sure our array mape directly to the IROps enum
static_assert(IRSizes[OP_LASTOP] == -1ULL);

std::string_view const& GetName(IROps Op);
static size_t GetSize(IROps Op) { return IRSizes[Op]; }

void Dump(IntrusiveIRList const* IR);
}
