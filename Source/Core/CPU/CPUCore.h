#pragma once
#include "Core/CPU/BlockCache.h"
#include "Core/CPU/CPUBackend.h"
#include "Core/CPU/CPUState.h"
#include "Core/CPU/PassManager.h"
#include "Core/CPU/OpcodeDispatch.h"
#include "Core/HLE/Syscalls/Syscalls.h"
#include "Core/Memmap.h"
#include <memory>
#include <unicorn/unicorn.h>

namespace Emu {
class CPUCore {
public:
friend SyscallHandler;
  CPUCore(Memmap *Mapper);
  void SetGS(uint64_t Value);
  void SetFS(uint64_t Value);
  void *MapRegion(uint64_t Offset, uint64_t Size);
  Emu::IR::IntrusiveIRList const* GetIRList(uint64_t Address) {
    return &irlists.at(Address);
  }

  void Init();
  void RunLoop();
  void FallbackToUnicorn();
  X86State CPUState{};
  uc_engine *uc;

	SyscallHandler syscallhandler;
  Memmap *MemoryMapper;
private:
  void SetGS();
  void SetFS();

  std::pair<BlockCache::BlockCacheIter, bool> CompileBlock();
  IR::OpDispatchBuilder OpDispatcher;
  bool StopRunning = false;
  std::vector<uc_hook> hooks;
  BlockCache blockcache;
  std::map<uint64_t, Emu::IR::IntrusiveIRList> irlists;
  uint32_t MaxBlockInstructions = 1;

  struct PassManagers {
    IR::BlockPassManager BlockManager;
    IR::FunctionPassManager FunctionManager;
  };
  PassManagers AnalysisPasses;
  PassManagers OptimizationPasses;
  std::unique_ptr<CPUBackend> Backend;
};
}
