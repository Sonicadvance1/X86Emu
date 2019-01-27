#pragma once
#include "Core/CPU/BlockCache.h"
#include "Core/CPU/CPUBackend.h"
#include "Core/CPU/CPUState.h"
#include "Core/CPU/PassManager.h"
#include "Core/CPU/OpcodeDispatch.h"
#include "Core/HLE/Syscalls/Syscalls.h"
#include "Core/Memmap.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <unicorn/unicorn.h>

namespace Emu {
class CPUCore final {
public:
friend SyscallHandler;
  CPUCore(Memmap *Mapper);

  struct ThreadState {
    ThreadState(CPUCore *cpu)
      : OpDispatcher{cpu} {}
    uc_engine *uc;
    std::vector<uc_hook> hooks;
    std::map<uint64_t, Emu::IR::IntrusiveIRList> irlists;
    BlockCache blockcache;
    std::thread ExecutionThread;
    X86State CPUState{};
    ThreadManagement threadmanager;
    IR::OpDispatchBuilder OpDispatcher;
    uint64_t JITRIP;
    std::condition_variable StartRunning;
    std::mutex StartRunningMutex;
    std::atomic<bool> ShouldStart;
    std::atomic<bool> StopRunning;
  };

  std::atomic<bool> PauseThreads{false};
  std::atomic<uint32_t> NumThreadsPaused;
  void Init(std::string const &File);
  void RunLoop();
  uint64_t lastThreadID = 0;
  std::vector<ThreadState*> Threads;
  ThreadState *ParentThread;
  std::mutex CPUThreadLock;

  static ThreadState *GetTLSThread();

  Emu::IR::IntrusiveIRList const* GetIRList(ThreadState *Thread, uint64_t Address) {
    return &Thread->irlists.at(Address);
  }
  void SetGS(ThreadState *Thread, uint64_t Value);
  void SetFS(ThreadState *Thread, uint64_t Value);
  void *MapRegion(ThreadState *Thread, uint64_t Offset, uint64_t Size);
  void MapRegionOnAll(uint64_t Offset, uint64_t Size);

  void FallbackToUnicorn(ThreadState *Thread);

  ThreadState *NewThread(X86State *NewState, uint64_t parent_tid, uint64_t child_tid);

  Memmap *MemoryMapper;
	SyscallHandler syscallhandler;
private:
  void InitThread(std::string const &File);
  void ExecutionThread(ThreadState *Thread);
  void SetGS(ThreadState *Thread);
  void SetFS(ThreadState *Thread);

  std::pair<BlockCache::BlockCacheIter, bool> CompileBlock(ThreadState *Thread);
  std::atomic<bool> StopRunning {false};
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
