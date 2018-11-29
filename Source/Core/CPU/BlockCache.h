#pragma once
#include "LogManager.h"
#include <map>

namespace Emu {
class BlockCache {
public:
  using BlockCacheType = std::map<uint64_t, void*>;
  using BlockCacheIter = BlockCacheType::iterator const;

  BlockCacheIter FindBlock(uint64_t Address) {
    return Blocks.find(Address);
  }

  BlockCacheIter End() { return Blocks.end(); }

  BlockCacheIter AddBlockMapping(uint64_t Address, void *Ptr) {
    auto ret = Blocks.insert(std::make_pair(Address, Ptr));
    LogMan::Throw::A(ret.second, "Couldn't insert block");
    return ret.first;
  }

  size_t Size() const { return Blocks.size(); }

private:
  BlockCacheType Blocks;
};
}
