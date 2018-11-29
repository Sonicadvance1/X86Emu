#pragma once

#include <string>
#include <vector>

namespace Emu {
class ELFLoader {
public:
bool Load(std::string const &File);
bool SetArguments(std::vector<std::string> const &Args);
};
}
