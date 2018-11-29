#include "Bootloader.h"
#include "ELFLoader.h"
#include "LogManager.h"

namespace Emu {

Bootloader::Bootloader() = default;
Bootloader::~Bootloader() = default;

bool Bootloader::Load(std::string const &File, std::vector<std::string> const &Args) {
  if (Loader) {
    LogMan::Msg::E("Loader was already loaded");
    return false;
  }

  Loader = std::make_unique<ELFLoader>();

  bool Result = true;
  Result &= Loader->Load(File);
  Result &= Loader->SetArguments(Args);
  return Result;
}

}
