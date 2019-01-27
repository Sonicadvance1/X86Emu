#include "LogManager.h"
#include "X86Tables.h"
#include <array>
#include <tuple>
#include <vector>

namespace Emu {
namespace X86Tables {

std::array<X86InstInfo, 255> BaseOps;
std::array<X86InstInfo, 255> SecondBaseOps;
std::array<X86InstInfo, (1 << 16) - 1> ModRMOps;

void InitializeInfoTables() {
  auto UnknownOp = X86InstInfo{"UND", TYPE_UNKNOWN, FLAGS_NONE, 0, 0};
  for (uint32_t i = 0; i < BaseOps.size(); ++i) {
    BaseOps[i] = UnknownOp;
  }
  for (uint32_t i = 0; i < SecondBaseOps.size(); ++i) {
    SecondBaseOps[i] = UnknownOp;
  }
  for (uint32_t i = 0; i < ModRMOps.size(); ++i) {
    ModRMOps[i] = UnknownOp;
  }

  const std::vector<std::tuple<uint8_t, uint8_t, X86InstInfo>> BaseOpTable = {
    // Prefixes
    {0x66, 1, X86InstInfo{"",      TYPE_LEGACY_PREFIX, FLAGS_NONE, 0, 0}},
    {0x67, 1, X86InstInfo{"",      TYPE_LEGACY_PREFIX, FLAGS_NONE, 0, 0}},
    {0x2E, 1, X86InstInfo{"CS",    TYPE_LEGACY_PREFIX, FLAGS_NONE, 0, 0}},
    {0x3E, 1, X86InstInfo{"DS",    TYPE_LEGACY_PREFIX, FLAGS_NONE, 0, 0}},
    {0x26, 1, X86InstInfo{"ES",    TYPE_LEGACY_PREFIX, FLAGS_NONE, 0, 0}},
    {0x64, 1, X86InstInfo{"FS",    TYPE_LEGACY_PREFIX, FLAGS_NONE, 0, 0}},
    {0x65, 1, X86InstInfo{"GS",    TYPE_LEGACY_PREFIX, FLAGS_NONE, 0, 0}},
    {0x36, 1, X86InstInfo{"SS",    TYPE_LEGACY_PREFIX, FLAGS_NONE, 0, 0}},
    {0xF0, 1, X86InstInfo{"LOCK",  TYPE_LEGACY_PREFIX, FLAGS_NONE, 0, 0}},
    {0xF2, 1, X86InstInfo{"REP",   TYPE_LEGACY_PREFIX, FLAGS_NONE, 0, 0}},
    {0xF3, 1, X86InstInfo{"REPNZ", TYPE_LEGACY_PREFIX, FLAGS_NONE, 0, 0}},

    // REX
    {0x40, 16, X86InstInfo{"", TYPE_REX_PREFIX, FLAGS_NONE, 0, 0}},

    // Instructions
    {0x01, 1, X86InstInfo{"ADD",    TYPE_INST, FLAGS_DST_MODRM | FLAGS_HAS_MODRM | FLAGS_DISPLACE_SIZE_DIV_2, 0, 0}},
    {0x03, 1, X86InstInfo{"ADD",    TYPE_INST, FLAGS_SRC_MODRM | FLAGS_HAS_MODRM,                             0, 0}},
    {0x05, 1, X86InstInfo{"ADD",    TYPE_INST, FLAGS_SRC_IMM | FLAGS_DISPLACE_SIZE_DIV_2,                     4, 0}},
    {0x08, 4, X86InstInfo{"OR",     TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0x20, 4, X86InstInfo{"AND",    TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0x25, 1, X86InstInfo{"AND",    TYPE_INST, FLAGS_DISPLACE_SIZE_DIV_2, 4, 0}},
    {0x29, 1, X86InstInfo{"SUB",    TYPE_INST, FLAGS_HAS_MODRM | FLAGS_DISPLACE_SIZE_DIV_2, 0, 0}},
    {0x2B, 1, X86InstInfo{"SUB",    TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0x2C, 1, X86InstInfo{"SUB",    TYPE_INST, FLAGS_NONE,                1, 0}},
    {0x2D, 1, X86InstInfo{"SUB",    TYPE_INST, FLAGS_DISPLACE_SIZE_DIV_2, 4, 0}},
    {0x30, 1, X86InstInfo{"XOR",    TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0x31, 1, X86InstInfo{"XOR",    TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0x32, 1, X86InstInfo{"XOR",    TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0x33, 1, X86InstInfo{"XOR",    TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0x38, 4, X86InstInfo{"CMP",    TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0x3C, 1, X86InstInfo{"CMP",    TYPE_INST, FLAGS_NONE,                1, 0}},
    {0x3D, 1, X86InstInfo{"CMP",    TYPE_INST, FLAGS_REX_IN_BYTE | FLAGS_DISPLACE_SIZE_DIV_2, 4, 0}},
    {0x50, 8, X86InstInfo{"PUSH",   TYPE_INST, FLAGS_REX_IN_BYTE,         0, 0}},
    {0x58, 8, X86InstInfo{"POP",    TYPE_INST, FLAGS_REX_IN_BYTE,         0, 0}},
    {0x63, 1, X86InstInfo{"MOVSXD", TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0x69, 1, X86InstInfo{"IMUL",   TYPE_INST, FLAGS_HAS_MODRM | FLAGS_DISPLACE_SIZE_DIV_2,   4, 0}},
    {0x70, 1, X86InstInfo{"JO",     TYPE_INST, FLAGS_NONE,                1, 0}},
    {0x71, 1, X86InstInfo{"JNO",    TYPE_INST, FLAGS_NONE,                1, 0}},
    {0x72, 1, X86InstInfo{"JB",     TYPE_INST, FLAGS_NONE,                1, 0}},
    {0x73, 1, X86InstInfo{"JNB",    TYPE_INST, FLAGS_NONE,                1, 0}},
    {0x74, 1, X86InstInfo{"JZ",     TYPE_INST, FLAGS_NONE,                1, 0}},
    {0x75, 1, X86InstInfo{"JNZ",    TYPE_INST, FLAGS_NONE,                1, 0}},
    {0x76, 1, X86InstInfo{"JBE",    TYPE_INST, FLAGS_NONE,                1, 0}},
    {0x77, 1, X86InstInfo{"JNBE",   TYPE_INST, FLAGS_NONE,                1, 0}},
    {0x78, 1, X86InstInfo{"JS",     TYPE_INST, FLAGS_NONE,                1, 0}},
    {0x79, 1, X86InstInfo{"JNS",    TYPE_INST, FLAGS_NONE,                1, 0}},
    {0x7A, 1, X86InstInfo{"JP",     TYPE_INST, FLAGS_NONE,                1, 0}},
    {0x7B, 1, X86InstInfo{"JNP",    TYPE_INST, FLAGS_NONE,                1, 0}},
    {0x7C, 1, X86InstInfo{"JL",     TYPE_INST, FLAGS_NONE,                1, 0}},
    {0x7D, 1, X86InstInfo{"JNL",    TYPE_INST, FLAGS_NONE,                1, 0}},
    {0x7E, 1, X86InstInfo{"JLE",    TYPE_INST, FLAGS_NONE,                1, 0}},
    {0x7F, 1, X86InstInfo{"JNLE",   TYPE_INST, FLAGS_NONE,                1, 0}},
    {0x82, 1, X86InstInfo{"[INV]",  TYPE_INVALID, FLAGS_NONE,                0, 0}},
    {0x84, 2, X86InstInfo{"TEST",   TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0x88, 5, X86InstInfo{"MOV",    TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0x8E, 1, X86InstInfo{"MOV",    TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0x8D, 1, X86InstInfo{"LEA",    TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0x90, 1, X86InstInfo{"NOP",    TYPE_INST, FLAGS_NONE,                0, 0}},
    {0x98, 1, X86InstInfo{"CDQE",   TYPE_INST, FLAGS_NONE,                0, 0}},
    {0x99, 1, X86InstInfo{"CQO",    TYPE_INST, FLAGS_NONE,                0, 0}},
    {0xA0, 4, X86InstInfo{"MOV",    TYPE_INST, FLAGS_NONE,                0, 0}},
    {0xA8, 1, X86InstInfo{"TEST",   TYPE_INST, FLAGS_NONE,                1, 0}},
    {0xA9, 1, X86InstInfo{"TEST",   TYPE_INST, FLAGS_DISPLACE_SIZE_DIV_2, 4, 0}},
    {0xAA, 2, X86InstInfo{"STOS",   TYPE_INST, FLAGS_NONE,                0, 0}},

    {0xB0, 8, X86InstInfo{"MOV",    TYPE_INST, FLAGS_REX_IN_BYTE,         1, 0}},
    {0xB8, 8, X86InstInfo{"MOV",    TYPE_INST, FLAGS_REX_IN_BYTE | FLAGS_DISPLACE_SIZE_DIV_2 | FLAGS_DISPLACE_SIZE_MUL_2, 4, 0}},
    {0xC2, 2, X86InstInfo{"RET",    TYPE_INST, FLAGS_SETS_RIP | FLAGS_BLOCK_END,                0, 0}},
    {0xC4, 2, X86InstInfo{"[INV]",  TYPE_INVALID, FLAGS_NONE,                0, 0}},
    {0xC6, 1, X86InstInfo{"MOV",    TYPE_INST, FLAGS_HAS_MODRM,           1, 0}},
    {0xC7, 1, X86InstInfo{"MOV",    TYPE_INST, FLAGS_HAS_MODRM,           4, 0}},
    {0xD4, 3, X86InstInfo{"[INV]",  TYPE_INVALID, FLAGS_NONE,                0, 0}},

    {0xE8, 1, X86InstInfo{"CALL",   TYPE_INST, FLAGS_SETS_RIP | FLAGS_DISPLACE_SIZE_DIV_2 | FLAGS_BLOCK_END, 4, 0}},
    {0xE9, 1, X86InstInfo{"JMP",    TYPE_INST, FLAGS_SETS_RIP | FLAGS_DISPLACE_SIZE_DIV_2 | FLAGS_BLOCK_END, 4, 0}},
    {0xEB, 1, X86InstInfo{"JMP",    TYPE_INST, FLAGS_SETS_RIP | FLAGS_BLOCK_END,                             1, 0}},

    // ModRM table
    {0x80, 1, X86InstInfo{"",   TYPE_MODRM_TABLE_PREFIX, FLAGS_HAS_MODRM, 3, 0}},
    {0x81, 1, X86InstInfo{"",   TYPE_MODRM_TABLE_PREFIX, FLAGS_HAS_MODRM, 3, 0}},
    {0x83, 1, X86InstInfo{"",   TYPE_MODRM_TABLE_PREFIX, FLAGS_HAS_MODRM, 3, 0}},
    {0xC0, 1, X86InstInfo{"",   TYPE_MODRM_TABLE_PREFIX, FLAGS_HAS_MODRM, 3, 0}},
    {0xC1, 1, X86InstInfo{"",   TYPE_MODRM_TABLE_PREFIX, FLAGS_HAS_MODRM, 3, 0}},
    {0xD0, 4, X86InstInfo{"",   TYPE_MODRM_TABLE_PREFIX, FLAGS_HAS_MODRM, 3, 0}},
    {0xD8, 1, X86InstInfo{"",   TYPE_MODRM_TABLE_PREFIX, FLAGS_HAS_MODRM, 3, 0}},
    {0xF6, 2, X86InstInfo{"",   TYPE_MODRM_TABLE_PREFIX, FLAGS_HAS_MODRM, 3, 0}},
    {0xFF, 1, X86InstInfo{"",   TYPE_MODRM_TABLE_PREFIX, FLAGS_HAS_MODRM, 3, 0}},
  };

  const std::vector<std::tuple<uint8_t, uint8_t, X86InstInfo>> TwoByteOpTable = {
    // Instructions
    {0x05, 1, X86InstInfo{"SYSCALL",    TYPE_INST, FLAGS_BLOCK_END, 0, 0}},
    {0x1f, 1, X86InstInfo{"NOP",        TYPE_INST, FLAGS_HAS_MODRM, 0, 0}},
    {0x31, 1, X86InstInfo{"RDTSC",      TYPE_INST, FLAGS_NONE,      0, 0}},
    {0x40, 1, X86InstInfo{"CMOVO",      TYPE_INST, FLAGS_HAS_MODRM, 0, 0}},
    {0x41, 1, X86InstInfo{"CMOVNO",     TYPE_INST, FLAGS_HAS_MODRM, 0, 0}},
    {0x42, 1, X86InstInfo{"CMOVB",      TYPE_INST, FLAGS_HAS_MODRM, 0, 0}},
    {0x43, 1, X86InstInfo{"CMOVNB",     TYPE_INST, FLAGS_HAS_MODRM, 0, 0}},
    {0x44, 1, X86InstInfo{"CMOVZ",      TYPE_INST, FLAGS_HAS_MODRM, 0, 0}},
    {0x45, 1, X86InstInfo{"CMOVNZ",     TYPE_INST, FLAGS_HAS_MODRM, 0, 0}},
    {0x46, 1, X86InstInfo{"CMOVBE",     TYPE_INST, FLAGS_HAS_MODRM, 0, 0}},
    {0x47, 1, X86InstInfo{"CMOVNBE",    TYPE_INST, FLAGS_HAS_MODRM, 0, 0}},
    {0x48, 1, X86InstInfo{"CMOVS",      TYPE_INST, FLAGS_HAS_MODRM, 0, 0}},
    {0x49, 1, X86InstInfo{"CMOVNS",     TYPE_INST, FLAGS_HAS_MODRM, 0, 0}},
    {0x4A, 1, X86InstInfo{"CMOVP",      TYPE_INST, FLAGS_HAS_MODRM, 0, 0}},
    {0x4B, 1, X86InstInfo{"CMOVNP",     TYPE_INST, FLAGS_HAS_MODRM, 0, 0}},
    {0x4C, 1, X86InstInfo{"CMOVL",      TYPE_INST, FLAGS_HAS_MODRM, 0, 0}},
    {0x4D, 1, X86InstInfo{"CMOVNL",     TYPE_INST, FLAGS_HAS_MODRM, 0, 0}},
    {0x4E, 1, X86InstInfo{"CMOVLE",     TYPE_INST, FLAGS_HAS_MODRM, 0, 0}},
    {0x4F, 1, X86InstInfo{"CMOVNLE",    TYPE_INST, FLAGS_HAS_MODRM, 0, 0}},

    {0x6E, 1, X86InstInfo{"MOVD",    TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0x6F, 1, X86InstInfo{"MOVDQU",  TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},

    {0x7E, 1, X86InstInfo{"MOVD",    TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0x7F, 1, X86InstInfo{"MOVDQU",  TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0x80, 1, X86InstInfo{"JO",      TYPE_INST, FLAGS_DISPLACE_SIZE_DIV_2, 4, 0}},
    {0x81, 1, X86InstInfo{"JNO",     TYPE_INST, FLAGS_DISPLACE_SIZE_DIV_2, 4, 0}},
    {0x82, 1, X86InstInfo{"JB",      TYPE_INST, FLAGS_DISPLACE_SIZE_DIV_2, 4, 0}},
    {0x83, 1, X86InstInfo{"JNB",     TYPE_INST, FLAGS_DISPLACE_SIZE_DIV_2, 4, 0}},
    {0x84, 1, X86InstInfo{"JZ",      TYPE_INST, FLAGS_DISPLACE_SIZE_DIV_2, 4, 0}},
    {0x85, 1, X86InstInfo{"JNZ",     TYPE_INST, FLAGS_DISPLACE_SIZE_DIV_2, 4, 0}},
    {0x86, 1, X86InstInfo{"JBE",     TYPE_INST, FLAGS_DISPLACE_SIZE_DIV_2, 4, 0}},
    {0x87, 1, X86InstInfo{"JNBE",    TYPE_INST, FLAGS_DISPLACE_SIZE_DIV_2, 4, 0}},
    {0x88, 1, X86InstInfo{"JS",      TYPE_INST, FLAGS_DISPLACE_SIZE_DIV_2, 4, 0}},
    {0x89, 1, X86InstInfo{"JNS",     TYPE_INST, FLAGS_DISPLACE_SIZE_DIV_2, 4, 0}},
    {0x8A, 1, X86InstInfo{"JP",      TYPE_INST, FLAGS_DISPLACE_SIZE_DIV_2, 4, 0}},
    {0x8B, 1, X86InstInfo{"JNP",     TYPE_INST, FLAGS_DISPLACE_SIZE_DIV_2, 4, 0}},
    {0x8C, 1, X86InstInfo{"JL",      TYPE_INST, FLAGS_DISPLACE_SIZE_DIV_2, 4, 0}},
    {0x8D, 1, X86InstInfo{"JNL",     TYPE_INST, FLAGS_DISPLACE_SIZE_DIV_2, 4, 0}},
    {0x8E, 1, X86InstInfo{"JLE",     TYPE_INST, FLAGS_DISPLACE_SIZE_DIV_2, 4, 0}},
    {0x8F, 1, X86InstInfo{"JNLE",    TYPE_INST, FLAGS_DISPLACE_SIZE_DIV_2, 4, 0}},
    {0x90, 1, X86InstInfo{"SETO",    TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0x91, 1, X86InstInfo{"SETNO",   TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0x92, 1, X86InstInfo{"SETB",    TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0x93, 1, X86InstInfo{"SETNB",   TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0x94, 1, X86InstInfo{"SETZ",    TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0x95, 1, X86InstInfo{"SETNZ",   TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0x96, 1, X86InstInfo{"SETBE",   TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0x97, 1, X86InstInfo{"SETNBE",  TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0x98, 1, X86InstInfo{"SETS",    TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0x99, 1, X86InstInfo{"SETNS",   TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0x9A, 1, X86InstInfo{"SETP",    TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0x9B, 1, X86InstInfo{"SETNP",   TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0x9C, 1, X86InstInfo{"SETL",    TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0x9D, 1, X86InstInfo{"SETNL",   TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0x9E, 1, X86InstInfo{"SETLE",   TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0x9F, 1, X86InstInfo{"SETNLE",  TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0xA2, 1, X86InstInfo{"CPUID",   TYPE_INST, FLAGS_NONE,                0, 0}},
    {0xA3, 1, X86InstInfo{"BT",      TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0xAF, 1, X86InstInfo{"IMUL",    TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0xB0, 2, X86InstInfo{"CMPXCHG", TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0xBA, 1, X86InstInfo{"BT",      TYPE_INST, FLAGS_HAS_MODRM,           1, 0}},
    {0xB6, 2, X86InstInfo{"MOVZX",   TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0xBC, 2, X86InstInfo{"BSF",     TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0xBE, 2, X86InstInfo{"MOVSX",   TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},

    // SSE
    {0x10, 2, X86InstInfo{"MOVUPS",     TYPE_INST, FLAGS_HAS_MODRM, 0, 0}},
    {0x16, 2, X86InstInfo{"MOVHPS",     TYPE_INST, FLAGS_HAS_MODRM, 0, 0}},
    {0x29, 1, X86InstInfo{"MOVAPS",     TYPE_INST, FLAGS_HAS_MODRM, 0, 0}},
    {0xEB, 1, X86InstInfo{"POR",        TYPE_INST, FLAGS_HAS_MODRM, 0, 0}},

    // SSE2
    {0x60, 1, X86InstInfo{"PUNPCKLBW",    TYPE_INST, FLAGS_HAS_MODRM,          0, 0}},
    {0x61, 1, X86InstInfo{"PUNPCKLWD",    TYPE_INST, FLAGS_HAS_MODRM,          0, 0}},
    {0x62, 1, X86InstInfo{"PUNPCKLDQ",    TYPE_INST, FLAGS_HAS_MODRM,          0, 0}},
    {0x66, 1, X86InstInfo{"PCMPGTD",    TYPE_INST, FLAGS_HAS_MODRM,          0, 0}},
    {0x6A, 1, X86InstInfo{"PUNPCKHDQ", TYPE_INST, FLAGS_HAS_MODRM,          0, 0}},
    {0x6C, 1, X86InstInfo{"PUNPCKLQDQ", TYPE_INST, FLAGS_HAS_MODRM,          0, 0}},
    {0x6D, 1, X86InstInfo{"PUNPCKHQDQ", TYPE_INST, FLAGS_HAS_MODRM,          0, 0}},
    {0x70, 1, X86InstInfo{"PSHUFD",     TYPE_INST, FLAGS_HAS_MODRM,          1, 0}},
    {0x73, 1, X86InstInfo{"PSLLQ",      TYPE_INST, FLAGS_HAS_MODRM,          1, 0}},
    {0x74, 1, X86InstInfo{"PCMPEQB",    TYPE_INST, FLAGS_HAS_MODRM,          0, 0}},
    {0x72, 1, X86InstInfo{"PSLLD",      TYPE_INST, FLAGS_HAS_MODRM,          1, 0}},
    {0x76, 1, X86InstInfo{"PCMPEQD",    TYPE_INST, FLAGS_HAS_MODRM,          0, 0}},
    {0xD4, 1, X86InstInfo{"PADDQ",      TYPE_INST, FLAGS_HAS_MODRM,          0, 0}},
    {0xD6, 1, X86InstInfo{"MOVQ",       TYPE_INST, FLAGS_HAS_MODRM,          0, 0}},
    {0xD7, 1, X86InstInfo{"PMOVMSKB",   TYPE_INST, FLAGS_HAS_MODRM,          0, 0}},
    {0xEF, 1, X86InstInfo{"PXOR",       TYPE_INST, FLAGS_HAS_MODRM,          0, 0}},
    {0xFE, 1, X86InstInfo{"PADDD",       TYPE_INST, FLAGS_HAS_MODRM,          0, 0}},
  };

  const std::vector<std::tuple<uint16_t, uint8_t, X86InstInfo>> ModRMOpTable = {
    {0x8000, 1, X86InstInfo{"ADD",  TYPE_INST, FLAGS_DST_MODRM | FLAGS_SRC_IMM | FLAGS_HAS_MODRM,           1, 0}},
    {0x8001, 1, X86InstInfo{"OR",   TYPE_INST, FLAGS_DST_MODRM | FLAGS_SRC_IMM | FLAGS_HAS_MODRM,           1, 0}},
    {0x8002, 1, X86InstInfo{"ADC",  TYPE_INST, FLAGS_DST_MODRM | FLAGS_SRC_IMM | FLAGS_HAS_MODRM,           1, 0}},
    {0x8003, 1, X86InstInfo{"SBB",  TYPE_INST, FLAGS_DST_MODRM | FLAGS_SRC_IMM | FLAGS_HAS_MODRM,           1, 0}},
    {0x8004, 1, X86InstInfo{"AND",  TYPE_INST, FLAGS_DST_MODRM | FLAGS_SRC_IMM | FLAGS_HAS_MODRM,           1, 0}},
    {0x8005, 1, X86InstInfo{"SUB",  TYPE_INST, FLAGS_DST_MODRM | FLAGS_SRC_IMM | FLAGS_HAS_MODRM,           1, 0}},
    {0x8006, 1, X86InstInfo{"XOR",  TYPE_INST, FLAGS_DST_MODRM | FLAGS_SRC_IMM | FLAGS_HAS_MODRM,           1, 0}},
    {0x8007, 1, X86InstInfo{"CMP",  TYPE_INST, FLAGS_DST_MODRM | FLAGS_SRC_IMM | FLAGS_HAS_MODRM,           1, 0}},

    {0x8100, 1, X86InstInfo{"ADD",  TYPE_INST, FLAGS_HAS_MODRM | FLAGS_DISPLACE_SIZE_DIV_2,           4, 0}},
    {0x8101, 1, X86InstInfo{"OR",   TYPE_INST, FLAGS_HAS_MODRM | FLAGS_DISPLACE_SIZE_DIV_2,           4, 0}},
    {0x8102, 1, X86InstInfo{"ADC",  TYPE_INST, FLAGS_HAS_MODRM | FLAGS_DISPLACE_SIZE_DIV_2,           4, 0}},
    {0x8103, 1, X86InstInfo{"SBB",  TYPE_INST, FLAGS_HAS_MODRM | FLAGS_DISPLACE_SIZE_DIV_2,           4, 0}},
    {0x8104, 1, X86InstInfo{"AND",  TYPE_INST, FLAGS_HAS_MODRM | FLAGS_DISPLACE_SIZE_DIV_2,           4, 0}},
    {0x8105, 1, X86InstInfo{"SUB",  TYPE_INST, FLAGS_HAS_MODRM | FLAGS_DISPLACE_SIZE_DIV_2,           4, 0}},
    {0x8106, 1, X86InstInfo{"XOR",  TYPE_INST, FLAGS_HAS_MODRM | FLAGS_DISPLACE_SIZE_DIV_2,           4, 0}},
    {0x8107, 1, X86InstInfo{"CMP",  TYPE_INST, FLAGS_HAS_MODRM | FLAGS_DISPLACE_SIZE_DIV_2,           4, 0}},

    {0x8300, 1, X86InstInfo{"ADD",  TYPE_INST, FLAGS_DST_MODRM | FLAGS_SRC_IMM | FLAGS_HAS_MODRM,           1, 0}},
    {0x8301, 1, X86InstInfo{"OR",   TYPE_INST, FLAGS_DST_MODRM | FLAGS_SRC_IMM | FLAGS_HAS_MODRM,           1, 0}},
    {0x8302, 1, X86InstInfo{"ADC",  TYPE_INST, FLAGS_DST_MODRM | FLAGS_SRC_IMM | FLAGS_HAS_MODRM,           1, 0}},
    {0x8303, 1, X86InstInfo{"SBB",  TYPE_INST, FLAGS_DST_MODRM | FLAGS_SRC_IMM | FLAGS_HAS_MODRM,           1, 0}},
    {0x8304, 1, X86InstInfo{"AND",  TYPE_INST, FLAGS_DST_MODRM | FLAGS_SRC_IMM | FLAGS_HAS_MODRM,           1, 0}},
    {0x8305, 1, X86InstInfo{"SUB",  TYPE_INST, FLAGS_DST_MODRM | FLAGS_SRC_IMM | FLAGS_HAS_MODRM,           1, 0}},
    {0x8306, 1, X86InstInfo{"XOR",  TYPE_INST, FLAGS_DST_MODRM | FLAGS_SRC_IMM | FLAGS_HAS_MODRM,           1, 0}},
    {0x8307, 1, X86InstInfo{"CMP",  TYPE_INST, FLAGS_DST_MODRM | FLAGS_SRC_IMM | FLAGS_HAS_MODRM,           1, 0}},

    {0xC000, 1, X86InstInfo{"ROL",  TYPE_INST, FLAGS_DST_MODRM | FLAGS_SRC_IMM | FLAGS_HAS_MODRM,           1, 0}},
    {0xC001, 1, X86InstInfo{"ROR",  TYPE_INST, FLAGS_DST_MODRM | FLAGS_SRC_IMM | FLAGS_HAS_MODRM,           1, 0}},
    {0xC002, 1, X86InstInfo{"RCL",  TYPE_INST, FLAGS_DST_MODRM | FLAGS_SRC_IMM | FLAGS_HAS_MODRM,           1, 0}},
    {0xC003, 1, X86InstInfo{"RCR",  TYPE_INST, FLAGS_DST_MODRM | FLAGS_SRC_IMM | FLAGS_HAS_MODRM,           1, 0}},
    {0xC004, 1, X86InstInfo{"SHL",  TYPE_INST, FLAGS_DST_MODRM | FLAGS_SRC_IMM | FLAGS_HAS_MODRM,           1, 0}},
    {0xC005, 1, X86InstInfo{"SHR",  TYPE_INST, FLAGS_DST_MODRM | FLAGS_SRC_IMM | FLAGS_HAS_MODRM,           1, 0}},
    {0xC006, 1, X86InstInfo{"SHL",  TYPE_INST, FLAGS_DST_MODRM | FLAGS_SRC_IMM | FLAGS_HAS_MODRM,           1, 0}},
    {0xC007, 1, X86InstInfo{"SAR",  TYPE_INST, FLAGS_DST_MODRM | FLAGS_SRC_IMM | FLAGS_HAS_MODRM,           1, 0}},

    {0xC100, 1, X86InstInfo{"ROL",  TYPE_INST, FLAGS_DST_MODRM | FLAGS_SRC_IMM | FLAGS_HAS_MODRM,           1, 0}},
    {0xC101, 1, X86InstInfo{"ROR",  TYPE_INST, FLAGS_DST_MODRM | FLAGS_SRC_IMM | FLAGS_HAS_MODRM,           1, 0}},
    {0xC102, 1, X86InstInfo{"RCL",  TYPE_INST, FLAGS_DST_MODRM | FLAGS_SRC_IMM | FLAGS_HAS_MODRM,           1, 0}},
    {0xC103, 1, X86InstInfo{"RCR",  TYPE_INST, FLAGS_DST_MODRM | FLAGS_SRC_IMM | FLAGS_HAS_MODRM,           1, 0}},
    {0xC104, 1, X86InstInfo{"SHL",  TYPE_INST, FLAGS_DST_MODRM | FLAGS_SRC_IMM | FLAGS_HAS_MODRM,           1, 0}},
    {0xC105, 1, X86InstInfo{"SHR",  TYPE_INST, FLAGS_DST_MODRM | FLAGS_SRC_IMM | FLAGS_HAS_MODRM,           1, 0}},
    {0xC106, 1, X86InstInfo{"SHL",  TYPE_INST, FLAGS_DST_MODRM | FLAGS_SRC_IMM | FLAGS_HAS_MODRM,           1, 0}},
    {0xC107, 1, X86InstInfo{"SAR",  TYPE_INST, FLAGS_DST_MODRM | FLAGS_SRC_IMM | FLAGS_HAS_MODRM,           1, 0}},

    {0xD000, 1, X86InstInfo{"ROL",  TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0xD001, 1, X86InstInfo{"ROR",  TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0xD002, 1, X86InstInfo{"RCL",  TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0xD003, 1, X86InstInfo{"RCR",  TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0xD004, 1, X86InstInfo{"SHL",  TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0xD005, 1, X86InstInfo{"SHR",  TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0xD006, 1, X86InstInfo{"SHL",  TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0xD007, 1, X86InstInfo{"SAR",  TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},

    {0xD100, 1, X86InstInfo{"ROL",  TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0xD101, 1, X86InstInfo{"ROR",  TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0xD102, 1, X86InstInfo{"RCL",  TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0xD103, 1, X86InstInfo{"RCR",  TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0xD104, 1, X86InstInfo{"SHL",  TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0xD105, 1, X86InstInfo{"SHR",  TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0xD106, 1, X86InstInfo{"SHL",  TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0xD107, 1, X86InstInfo{"SAR",  TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},

    {0xD200, 1, X86InstInfo{"ROL",  TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0xD201, 1, X86InstInfo{"ROR",  TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0xD202, 1, X86InstInfo{"RCL",  TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0xD203, 1, X86InstInfo{"RCR",  TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0xD204, 1, X86InstInfo{"SHL",  TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0xD205, 1, X86InstInfo{"SHR",  TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0xD206, 1, X86InstInfo{"SHL",  TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0xD207, 1, X86InstInfo{"SAR",  TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},

    {0xD300, 1, X86InstInfo{"ROL",  TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0xD301, 1, X86InstInfo{"ROR",  TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0xD302, 1, X86InstInfo{"RCL",  TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0xD303, 1, X86InstInfo{"RCR",  TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0xD304, 1, X86InstInfo{"SHL",  TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0xD305, 1, X86InstInfo{"SHR",  TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0xD306, 1, X86InstInfo{"SHL",  TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0xD307, 1, X86InstInfo{"SAR",  TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},

    {0xF600, 2, X86InstInfo{"TEST", TYPE_INST, FLAGS_HAS_MODRM, 1, 0}},
    {0xF604, 1, X86InstInfo{"MUL",  TYPE_INST, FLAGS_HAS_MODRM, 0, 0}},
    {0xF606, 1, X86InstInfo{"DIV",  TYPE_INST, FLAGS_HAS_MODRM, 1, 0}},
    {0xF700, 2, X86InstInfo{"TEST", TYPE_INST, FLAGS_HAS_MODRM | FLAGS_DISPLACE_SIZE_DIV_2, 4, 0}},
    {0xF702, 1, X86InstInfo{"NOT",  TYPE_INST, FLAGS_HAS_MODRM, 0, 0}},
    {0xF703, 1, X86InstInfo{"NEG",  TYPE_INST, FLAGS_HAS_MODRM, 0, 0}},
    {0xF704, 1, X86InstInfo{"MUL",  TYPE_INST, FLAGS_HAS_MODRM, 0, 0}},
    {0xF705, 1, X86InstInfo{"IMUL", TYPE_INST, FLAGS_HAS_MODRM, 0, 0}},
    {0xF706, 1, X86InstInfo{"DIV",  TYPE_INST, FLAGS_HAS_MODRM, 0, 0}},
    {0xF707, 1, X86InstInfo{"IDIV", TYPE_INST, FLAGS_HAS_MODRM, 0, 0}},

    {0xFF00, 1, X86InstInfo{"INC",   TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0xFF01, 1, X86InstInfo{"DEC",   TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
    {0xFF02, 1, X86InstInfo{"CALL",  TYPE_INST, FLAGS_SETS_RIP | FLAGS_HAS_MODRM | FLAGS_BLOCK_END,           0, 0}},
    {0xFF03, 1, X86InstInfo{"CALLF", TYPE_INST, FLAGS_SETS_RIP | FLAGS_HAS_MODRM | FLAGS_BLOCK_END,           0, 0}},
    {0xFF04, 1, X86InstInfo{"JMP",   TYPE_INST, FLAGS_SETS_RIP | FLAGS_HAS_MODRM | FLAGS_BLOCK_END,           0, 0}},
    {0xFF05, 1, X86InstInfo{"JMPF",  TYPE_INST, FLAGS_SETS_RIP | FLAGS_HAS_MODRM | FLAGS_BLOCK_END,           0, 0}},
    {0xFF06, 1, X86InstInfo{"PUSH",  TYPE_INST, FLAGS_HAS_MODRM,           0, 0}},
  };

  auto GenerateTable = [](auto& FinalTable, auto& LocalTable) {
    for (auto Op : LocalTable) {
      auto OpNum = std::get<0>(Op);
      auto Info = std::get<2>(Op);
      for (uint8_t i = 0; i < std::get<1>(Op); ++i) {
        LogMan::Throw::A(FinalTable[OpNum + i].Type == TYPE_UNKNOWN, "Duplicate Entry");
        FinalTable[OpNum + i] = Info;
      }
    }
  };

  GenerateTable(BaseOps, BaseOpTable);
  GenerateTable(SecondBaseOps, TwoByteOpTable);
  GenerateTable(ModRMOps, ModRMOpTable);
}

DecodedOp GetInstInfo(uint8_t const *Inst) {
  X86InstInfo *Info{nullptr};
  X86InstDecodeFlags Flags{};
  uint8_t InstructionSize = 0;
  std::array<uint8_t, 15> Instruction;

  bool DoAgain = false;
  uint8_t REXOptions = 0;
  bool HasWideningDisplacement = false;
  bool HasNarrowingDisplacement = false;
OnceMore:

  uint8_t AdditionalBytes = 0;
  Instruction[InstructionSize] = Inst[InstructionSize]; InstructionSize++;
  uint8_t Op = Inst[InstructionSize - 1];

  auto NormalOp = [&](auto &Table, auto Op) {
    Info = &Table[Op];

    // We hit a prefix, go again
    if (Info->Type == TYPE_PREFIX || Info->Type == TYPE_LEGACY_PREFIX) {
      DoAgain = true;
      return;
    }

    bool HasMODRM = (Info->Flags & FLAGS_HAS_MODRM) || (REXOptions & 1);

    if (Info->Flags & FLAGS_REX_IN_BYTE)
      HasMODRM = false;

    bool HasSIB = REXOptions & 2;
    uint8_t Disp = 0;

    if (HasMODRM) {
      union {
        uint8_t Hex;
        struct {
          uint8_t rm : 3;
          uint8_t reg : 3;
          uint8_t mod : 2;
        };
      } ModRM;
      ModRM.Hex = Inst[InstructionSize];

      // if ModRM.Mod != 0b11
      // AND
      // ModRM.rm == 0b100
      HasSIB = HasSIB ||
               ((ModRM.mod != 0b11) &&
                (ModRM.rm == 0b100));

      // Do we have an offset?
      if (ModRM.mod == 0b01)
        Disp = 1;
      else if (ModRM.mod == 0b10)
        Disp = 4;
      else if (ModRM.mod == 0 && ModRM.rm == 0b101)
        Disp = 4;

      ;//printf("ModRM: 0x%02x HasSIB? %s Disp: %d %d %d %d\n", ModRM.Hex, HasSIB ? "Yes" : "No", Disp, ModRM.mod, ModRM.reg, ModRM.rm);
      if (HasSIB) {
        union {
          uint8_t Hex;
          struct {
            uint8_t base : 3;
            uint8_t index : 3;
            uint8_t scale : 2;
          };
        } SIB;
        SIB.Hex = Inst[InstructionSize+1];
        ;//printf("\tSIB: 0x%02x %d %d %d\n", SIB.Hex, SIB.base, SIB.index, SIB.scale);

        // If the SIB base is 0b101, aka BP or R13 then we have a 32bit displacement
        if (SIB.base == 0b101 && Disp == 0) {
          ;//printf("\tDisp now 4\n");
          Disp = 4;
        }
      }
    }

    if (HasMODRM) { // MODRM
      AdditionalBytes++;
      Flags.Flags |= DECODE_FLAG_MODRM;
    }

    if (HasSIB) {
      AdditionalBytes++;
      Flags.Flags |= DECODE_FLAG_SIB;
    }

    AdditionalBytes += Disp;

    uint8_t Bytes = Info->MoreBytes;
    if ((Info->Flags & FLAGS_DISPLACE_SIZE_MUL_2) && HasWideningDisplacement) {
      ;//printf("Cool, nondefault displacement\n");
      Bytes *= 2;
    }
    if ((Info->Flags & FLAGS_DISPLACE_SIZE_DIV_2) && HasNarrowingDisplacement) {
      ;//printf("Cool, nondefault displacement\n");
      Bytes /= 2;
    }

    AdditionalBytes += Bytes;
  };

  auto ModRMOp = [&]() {
    uint8_t Prefix = Inst[InstructionSize - 1];
    uint8_t ModRM = Inst[InstructionSize];

    // Get the Prefix Info
    auto PrefixInfo = &BaseOps[Prefix];
    uint8_t ValidModRMMask = (1 << PrefixInfo->MoreBytes) - 1;
    uint16_t Op = (Prefix << 8) | (((ModRM & 0b111000) >> 3) & (ValidModRMMask));
    ;//printf("ModRM Op: 0x%04x\n", Op);

    // Find the instruction Info
    NormalOp(ModRMOps, Op);
  };


  switch (Op) {
  case 0x0F: { // Escape
    Flags.PrefixBytes++;
    Instruction[InstructionSize] = Inst[InstructionSize]; InstructionSize++;

    switch (Inst[InstructionSize]) {
    case 0x0F: // Escape
      ;//printf("3DNow!\n");
      Flags.PrefixBytes++;
      // This turns in to ModRM
      Instruction[InstructionSize] = Inst[InstructionSize]; InstructionSize++;
    break;
    case 0x38: // Escape
      ;//printf("F38 Table!\n");
      Flags.PrefixBytes++;
    break;
    case 0x3A: // Escape
      ;//printf("F3A Table!\n");
      Flags.PrefixBytes++;
    break;
    default: // Two Byte op table
      ;//printf("TwoByte Op Table!\n");
      NormalOp(SecondBaseOps, Inst[InstructionSize - 1]);
    break;
    }
  }
  break;

  case 0x66: // Operand size prefix
    ;//printf("Operand size prefix!\n");
    // 66 83 78 36 38          cmp    WORD PTR [rax+0x36],0x38
    HasNarrowingDisplacement = true;
    Flags.PrefixBytes++;
    Flags.Flags |= DECODE_FLAG_OPSIZE;
    DoAgain = true;
  break;
  case 0x67: // Address size override prefix
    ;//printf("Address size override prefix\n");
    Flags.PrefixBytes++;
    Flags.Flags |= DECODE_FLAG_ADSIZE;
    DoAgain = true;
  break;
  case 0x40: // REX - 0x40-0x4F
  case 0x41:
  case 0x42:
  case 0x43:
  case 0x44:
  case 0x45:
  case 0x46:
  case 0x47:
  case 0x48:
  case 0x49:
  case 0x4A:
  case 0x4B:
  case 0x4C:
  case 0x4D:
  case 0x4E:
  case 0x4F: {
    ;//printf("REX Op\n");
    uint8_t REXValue = Inst[InstructionSize - 1];
    Info = &BaseOps[Inst[InstructionSize - 1]];

    REXOptions |= 0b100;
    // ModRM
    if (REXValue & 0b0101)
      REXOptions |= 0b001;

    // SIB
    if (REXValue & 0b0010)
      REXOptions |= 0b010;

    // XXX: Throw in to REXOptions
    if (REXValue & 0b1000)
      HasWideningDisplacement = true;

    Flags.Flags |= DECODE_FLAG_REX;
    ;//printf("REX 0x%01x\n", REXValue & 0xF);

    DoAgain = true;
  }
  break;
  case 0xF0: {
    Flags.Flags |= DECODE_FLAG_LOCK;
    DoAgain = true;
  }
  break;
  default: // Default Base Op
    ;//printf("Default op! 0x%02x\n", Op);
    Info = &BaseOps[Inst[InstructionSize - 1]];
    if (Info->Type == TYPE_MODRM_TABLE_PREFIX) {
      ModRMOp();
    }
    else {
      NormalOp(BaseOps, Inst[InstructionSize - 1]);
    }
  break;
  }

  // Read any additional bytes specified
  for (uint32_t i = 0; i < AdditionalBytes; ++i) {
    Instruction[InstructionSize] = Inst[InstructionSize]; InstructionSize++;
  }

  if (DoAgain) {
    DoAgain = false;
    goto OnceMore;
  }

//  printf("Instruction: ");
//  if (Info) {
//    printf("%s :", Info->Name);
//  }
//
//  for (uint8_t i = 0; i < InstructionSize; ++i) {
//    printf("%02x ", Instruction[i]);
//  }
//  printf("\n");
  Flags.Size = InstructionSize;
  return std::make_pair(Info ? Info->Type == TYPE_UNKNOWN ? nullptr : Info : nullptr, Flags);
}
}
}
