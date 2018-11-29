#include "Core/CPU/IR.h"

namespace Emu::IR {
std::array<std::string, OP_LASTOP + 1> IRNames = {
	"Constant", // sizeof(IROp_Constant),
	"LoadContext", // sizeof(IROp_LoadContext),
	"StoreContext", // sizeof(IROp_StoreContext),
  "BeginBlock", // sizeof(IROp_BeginBlock), // BeginBlock
  "EndBlock", // sizeof(IROp_EndBlock), // EndBlock

  // Instructions
  "Add", // sizeof(IROp_Add),
  "Xor", // sizeof(IROp_Xor),
  "Trunc_32", // sizeof(IROp_Trunc_32),
	"Syscall", // sizeof(IROp_Syscall),
};

std::string& GetName(IROps Op) {
  return IRNames[Op];
}

}
