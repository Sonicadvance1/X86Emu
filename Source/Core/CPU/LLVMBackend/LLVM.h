#pragma once
#include "Core/CPU/CPUBackend.h"

namespace Emu {
class CPUCore;
CPUBackend *CreateLLVMBackend(Emu::CPUCore *CPU);
}
