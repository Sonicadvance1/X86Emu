#pragma once
#include "Core/CPU/CPUBackend.h"

namespace Emu {
class Interpreter final : public CPUBackend {
public:
  std::string GetName() override { return "Interpreter"; }
  void* CompileCode(Emu::IR::IntrusiveIRList const *ir) override;
private:


};
}
