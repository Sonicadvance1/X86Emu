#pragma once
#include "IntrusiveIRList.h"
#include <string>

namespace Emu {
class CPUBackend {
public:
  virtual ~CPUBackend() = default;
  virtual std::string GetName() = 0;
  virtual void* CompileCode(Emu::IR::IntrusiveIRList const *ir) = 0;
};
}
