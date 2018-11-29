#pragma once
#include "Core/CPU/CPUBackend.h"

namespace Emu {
class AArch64 final : public CPUBackend {
public:
  std::string GetName() override { return "AArch64"; }
  void* CompileCode(Emu::IR::IntrusiveIRList const *ir) override;
private:
};
}
