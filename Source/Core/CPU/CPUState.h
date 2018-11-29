#pragma once
#include <cstdint>

namespace Emu {

constexpr unsigned REG_RAX = 0;
constexpr unsigned REG_RBX = 1;
constexpr unsigned REG_RCX = 2;
constexpr unsigned REG_RDX = 3;
constexpr unsigned REG_RSI = 4;
constexpr unsigned REG_RDI = 5;
constexpr unsigned REG_RBP = 6;
constexpr unsigned REG_RSP = 7;
constexpr unsigned REG_R8  = 8;
constexpr unsigned REG_R9  = 9;
constexpr unsigned REG_R10 = 10;
constexpr unsigned REG_R11 = 11;
constexpr unsigned REG_R12 = 12;
constexpr unsigned REG_R13 = 13;
constexpr unsigned REG_R14 = 14;
constexpr unsigned REG_R15 = 15;

struct X86State {
  uint64_t rip;
  uint64_t gregs[16];
  uint64_t xmm[16][2];
  uint64_t gs;
  uint64_t fs;
};
}
