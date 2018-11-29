#pragma once

#include <memory>
#include <string>
#include <vector>

namespace Emu {
class ELFLoader;

class Bootloader {
public:
  Bootloader();
  ~Bootloader();
  bool Load(std::string const &File, std::vector<std::string> const &Args);

private:
  std::unique_ptr<ELFLoader> Loader;
};
}
