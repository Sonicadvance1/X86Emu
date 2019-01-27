#include "Bootloader/Bootloader.h"
#include "Core.h"
#include "ELFLoader.h"
#include "LogManager.h"
#include <unicorn/unicorn.h>
#include <unicorn/x86.h>

namespace Emu {

Core::Core()
  : CPU{&MemoryMapper} {
}

bool Core::Load(std::string const &File, std::vector<std::string> const &Args) {
  bool Result = true;
  // Allocate a 64GB SHM region for fun
  Result &= MemoryMapper.AllocateSHMRegion(1ULL << 33);

  CPU.Init(File);
  CPU.RunLoop();

  return Result;
}

}
