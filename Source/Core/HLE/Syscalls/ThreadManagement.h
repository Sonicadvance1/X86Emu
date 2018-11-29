#pragma once
#include <cstdint>

namespace Emu {
// XXX: This should map multiple IDs correctly
// Tracking relationships between thread IDs and such
class ThreadManagement {
public:
  uint64_t GetUID()  { return 1; }
  uint64_t GetGID()  { return 1; }
  uint64_t GetEUID() { return 1; }
  uint64_t GetEGID() { return 1; }
  uint64_t GetTID()  { return 1; }
};
}
