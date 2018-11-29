#include "Bootloader/Bootloader.h"
#include "CPU/CPUCore.h"
#include "Memmap.h"
#include <string>
#include <vector>

namespace Emu {
class Core {
public:
  enum State {
    STATE_UNLOADED = 0,
    STATE_PAUSED,
    STATE_RUNNING,
  };

  Core();
  bool Load(std::string const &File, std::vector<std::string> const &Args);

private:
  Bootloader BL{};
  CPUCore CPU;
  Memmap MemoryMapper{};
};
}
