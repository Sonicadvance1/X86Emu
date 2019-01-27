#include "CPUCore.h"
#include "ELFLoader.h"
#include "IR.h"
#include "LogManager.h"
#include "OpcodeDispatch.h"
#include "X86Tables.h"
#include "AArch64Backend/AArch64.h"
#include "InterpreterBackend/Interpreter.h"
#include "LLVMBackend/LLVM.h"
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unordered_map>

static uint64_t AlignUp(uint64_t value, uint64_t size) {
  return value + (size - value % size) % size;
};
static uint64_t AlignDown(uint64_t value, uint64_t size) {
  return value - value % size;
};

constexpr uint64_t PAGE_SIZE = 4096;
constexpr uint64_t FS_OFFSET = 0xb000'0000;

constexpr uint64_t STACK_SIZE = 8 * 1024 * 1024;
constexpr uint64_t STACK_OFFSET  = 0xc000'0000;

static void hook_unmapped(uc_engine *uc, uc_mem_type type, uint64_t address,
                          int size, int64_t value, void *user_data) {
  uint64_t rip;

  Emu::CPUCore::ThreadState *state = (Emu::CPUCore::ThreadState*)user_data;

  uc_reg_read(uc, UC_X86_REG_RIP, &rip);
  printf(">>> %ld: RIP is 0x%" PRIx64 "\n", state->threadmanager.GetTID(), rip);

  printf("Attempted to access 0x%zx with type %d, size 0x%08x\n", address, type,
         size);

  uc_mem_region *regions;
  uint32_t regioncount;
  uc_mem_regions(uc, &regions, &regioncount);

  for (uint32_t i = 0; i < regioncount; ++i) {
    printf("%d: %zx - %zx\n", i, regions[i].begin, regions[i].end);
  }
}

uint64_t LastInstSize = 0;
std::atomic<bool> StopRunning {false};
bool SizesDidNotMatch = false;
static void hook_code64(uc_engine *uc, uint64_t address, uint32_t size,
                        void *user_data) {

  uint64_t rip;

  uc_reg_read(uc, UC_X86_REG_RIP, &rip);
  printf(">>> Tracing instruction at 0x%" PRIx64 ", instruction size = 0x%x\n",
         address, size);

  std::string Name = "???";
  printf(">>> RIP is 0x%" PRIx64 " %s\n", rip, Name.c_str());

  if (LastInstSize != size) {
    StopRunning = true;
    SizesDidNotMatch = true;
  }
}

namespace Emu {
void CPUCore::SetGS(ThreadState *Thread) {
  uc_x86_msr Val;
  Val.rid = 0xC0000101;

  Val.value = Thread->CPUState.gs;
  uc_reg_write(Thread->uc, UC_X86_REG_MSR, &Val);
}

void CPUCore::SetFS(ThreadState *Thread) {
  uc_x86_msr Val;
  Val.rid = 0xC0000100;
  Val.value = Thread->CPUState.fs;
  uc_reg_write(Thread->uc, UC_X86_REG_MSR, &Val);
}

void CPUCore::SetGS(ThreadState *Thread, uint64_t Value) {
  Thread->CPUState.gs = Value;
  SetGS(Thread);
}

void CPUCore::SetFS(ThreadState *Thread, uint64_t Value) {
  Thread->CPUState.fs = Value;
  SetFS(Thread);
}

void *CPUCore::MapRegion(ThreadState *Thread, uint64_t Offset, uint64_t Size) {
  void *Ptr = MemoryMapper->MapRegion(Offset, Size);

  uc_err err = uc_mem_map_ptr(Thread->uc, Offset, Size, UC_PROT_ALL, Ptr);
  if (err) {
    printf("Failed on uc_mem_map() with error returned %u: %s\n", err,
        uc_strerror(err));
  }
  return Ptr;
}

void CPUCore::MapRegionOnAll(uint64_t Offset, uint64_t Size) {
  PauseThreads = true;
  while (NumThreadsPaused.load() != (Threads.size() - 1));

  for (auto Thread : Threads) {
    MapRegion(Thread, Offset, Size);
  }
  PauseThreads = false;
}

CPUCore::CPUCore(Memmap *Mapper)
  : MemoryMapper{Mapper}
  , syscallhandler {this} {
  X86Tables::InitializeInfoTables();
  IR::InstallOpcodeHandlers();
}

void CPUCore::Init(std::string const &File) {
  // XXX: Allow runtime selection between cores
#if 1
  Backend.reset(CreateLLVMBackend(this));
#elif 0
  Backend.reset(new AArch64());
#else
  Backend.reset(new Interpreter());
#endif
  InitThread(File);

}

void CPUCore::RunLoop() {
  for (auto &Thread : Threads)
    Thread->ExecutionThread.join();
}

thread_local CPUCore::ThreadState* TLSThread;
CPUCore::ThreadState *CPUCore::GetTLSThread() {
  return TLSThread;
}

void CPUCore::InitThread(std::string const &File) {
  ThreadState *threadstate{nullptr};

  {
    std::lock_guard<std::mutex> lk(CPUThreadLock);
    threadstate = Threads.emplace_back(new ThreadState{this});
    threadstate->threadmanager.TID = ++lastThreadID;
  }

  threadstate->StopRunning = false;
  threadstate->ShouldStart = false;

  // Initialize default CPU state
  threadstate->CPUState.rip = ~0ULL;
  for (int i = 0; i < 16; ++i) {
    threadstate->CPUState.gregs[i] = 0;
  }
  for (int i = 0; i < 16; ++i) {
    threadstate->CPUState.xmm[i][0] = 0xDEADBEEFULL;
    threadstate->CPUState.xmm[i][1] = 0xBAD0DAD1ULL;
  }

  threadstate->CPUState.gs = 0;
  threadstate->CPUState.fs = 0;
  threadstate->CPUState.rflags = 2ULL; // Initializes to this value

  ::ELFLoader::ELFContainer file(File);

  uc_engine *uc;
  uc_err err;

  auto MemLayout = file.GetLayout();
  printf("Emulate x86_64 code\n");

  // Initialize emulator in X86-64bit mode
  err = uc_open(UC_ARCH_X86, UC_MODE_64, &uc);
  LogMan::Throw::A(!err, "Failed on uc_open()");
  LogMan::Throw::A(uc != 0, "Failed on uc_open()");

  threadstate->uc = uc;
  {
    uint64_t BasePtr = AlignDown(std::get<0>(MemLayout), PAGE_SIZE);
    uint64_t BaseSize = AlignUp(std::get<2>(MemLayout), PAGE_SIZE);

    MapRegion(threadstate, BasePtr, BaseSize);
    MapRegion(threadstate, STACK_OFFSET, STACK_SIZE);
    MapRegion(threadstate, FS_OFFSET, 0x1000);
  }

  uint64_t rsp = STACK_OFFSET + STACK_SIZE;

  const std::vector<uint8_t> Values = {
      2,    0,   0,   0,   0,   0, 0, 0, // Argument count
      0,    0,   0,   0,   0,   0, 0, 0, // Argument0 pointer
      0,    0,   0,   0,   0,   0, 0, 0, // Argument1 pointer
      'B',  'u', 't', 't', 's',          // Argument0
      '\0',
  };

  rsp -= Values.size() + 0x1000;
  uint64_t arg0offset = rsp + 8;
  uint64_t arg0value = rsp + 24;
  uc_mem_write(uc, rsp, &Values.at(0), Values.size());
  uc_mem_write(uc, arg0offset, &arg0value, 8);
  uc_mem_write(uc, arg0offset + 8, &arg0value, 8);

  auto Writer = [&](void *Data, uint64_t Addr, uint64_t Size) {
    // write machine code to be emulated to memory
    if (uc_mem_write(uc, Addr, Data, Size)) {
      LogMan::Msg::A("Failed to write emulation code to memory, quit!\n", "");
    }
  };

  file.WriteLoadableSections(Writer);

  threadstate->CPUState.gregs[REG_RSP] = rsp;
  threadstate->CPUState.rip = file.GetEntryPoint();

  uc_reg_write(threadstate->uc, UC_X86_REG_RSP, &threadstate->CPUState.gregs[REG_RSP]);
  uc_reg_write(threadstate->uc, UC_X86_REG_RIP, &threadstate->CPUState.rip);

  // tracing all instructions in the range [EntryPoint, EntryPoint+20]
 // uc_hook_add(uc, &hooks.emplace_back(), UC_HOOK_CODE, (void*)hook_code64, nullptr, 1, 0);

  uc_hook_add(threadstate->uc, &threadstate->hooks.emplace_back(), UC_HOOK_MEM_UNMAPPED, (void *)hook_unmapped, threadstate, 1,
              0);

  ParentThread = threadstate;
  // Kick off the execution thread
  threadstate->ExecutionThread = std::thread(&CPUCore::ExecutionThread, this, threadstate);
  std::lock_guard<std::mutex> lk(threadstate->StartRunningMutex);
  threadstate->ShouldStart = true;
  threadstate->StartRunning.notify_all();

}

CPUCore::ThreadState *CPUCore::NewThread(X86State *NewState, uint64_t parent_tid, uint64_t child_tid) {
  ThreadState *threadstate{nullptr};
  ThreadState *parenthread = GetTLSThread();

  {
    std::lock_guard<std::mutex> lk(CPUThreadLock);
    threadstate = Threads.emplace_back(new ThreadState{this});
    threadstate->threadmanager.TID = ++lastThreadID;
  }

  threadstate->StopRunning = false;
  threadstate->ShouldStart = false;
  // Initialize default CPU state
  // Since we are a new thread copy the data from the parent
  memcpy(&threadstate->CPUState, NewState, sizeof(X86State));

  threadstate->threadmanager.parent_tid = parent_tid;
  threadstate->threadmanager.child_tid = child_tid;

  uc_engine *uc;
  uc_err err;

  printf("Emulate x86_64 code\n");

  // Initialize emulator in X86-64bit mode
  err = uc_open(UC_ARCH_X86, UC_MODE_64, &uc);
  threadstate->uc = uc;
  LogMan::Throw::A(!err, "Failed on uc_open()");

  {
    // Copy the parent's memory mapping
    auto Regions = MemoryMapper->MappedRegions;
    for (auto const& region : Regions) {
      uc_err err = uc_mem_map_ptr(uc,
          region.Offset,
          region.Size,
          UC_PROT_ALL,
          region.Ptr);
      if (err) {
        printf("Failed on uc_mem_map() with error 0x%zx - 0x%zx from %p returned %u: %s\n",
            region.Offset,
            region.Offset + region.Size,
            region.Ptr,
            err,
            uc_strerror(err));
      }
    }
  }

  {
    std::array<int, 34> GPRs = {
      UC_X86_REG_RIP,
      UC_X86_REG_RAX,
      UC_X86_REG_RBX,
      UC_X86_REG_RCX,
      UC_X86_REG_RDX,
      UC_X86_REG_RSI,
      UC_X86_REG_RDI,
      UC_X86_REG_RBP,
      UC_X86_REG_RSP,
      UC_X86_REG_R8,
      UC_X86_REG_R9,
      UC_X86_REG_R10,
      UC_X86_REG_R11,
      UC_X86_REG_R12,
      UC_X86_REG_R13,
      UC_X86_REG_R14,
      UC_X86_REG_R15,
      UC_X86_REG_XMM0,
      UC_X86_REG_XMM1,
      UC_X86_REG_XMM2,
      UC_X86_REG_XMM3,
      UC_X86_REG_XMM4,
      UC_X86_REG_XMM5,
      UC_X86_REG_XMM6,
      UC_X86_REG_XMM7,
      UC_X86_REG_XMM8,
      UC_X86_REG_XMM9,
      UC_X86_REG_XMM10,
      UC_X86_REG_XMM11,
      UC_X86_REG_XMM12,
      UC_X86_REG_XMM13,
      UC_X86_REG_XMM14,
      UC_X86_REG_XMM15,
      UC_X86_REG_EFLAGS,
    };
    std::array<void*, GPRs.size()> GPRPointers = {
      &threadstate->CPUState.rip,
      &threadstate->CPUState.gregs[0],
      &threadstate->CPUState.gregs[1],
      &threadstate->CPUState.gregs[2],
      &threadstate->CPUState.gregs[3],
      &threadstate->CPUState.gregs[4],
      &threadstate->CPUState.gregs[5],
      &threadstate->CPUState.gregs[6],
      &threadstate->CPUState.gregs[7],
      &threadstate->CPUState.gregs[8],
      &threadstate->CPUState.gregs[9],
      &threadstate->CPUState.gregs[10],
      &threadstate->CPUState.gregs[11],
      &threadstate->CPUState.gregs[12],
      &threadstate->CPUState.gregs[13],
      &threadstate->CPUState.gregs[14],
      &threadstate->CPUState.gregs[15],
      &threadstate->CPUState.xmm[0],
      &threadstate->CPUState.xmm[1],
      &threadstate->CPUState.xmm[2],
      &threadstate->CPUState.xmm[3],
      &threadstate->CPUState.xmm[4],
      &threadstate->CPUState.xmm[5],
      &threadstate->CPUState.xmm[6],
      &threadstate->CPUState.xmm[7],
      &threadstate->CPUState.xmm[8],
      &threadstate->CPUState.xmm[9],
      &threadstate->CPUState.xmm[10],
      &threadstate->CPUState.xmm[11],
      &threadstate->CPUState.xmm[12],
      &threadstate->CPUState.xmm[13],
      &threadstate->CPUState.xmm[14],
      &threadstate->CPUState.xmm[15],
      &threadstate->CPUState.rflags,
    };
    static_assert(GPRs.size() == GPRPointers.size());

    uc_reg_write_batch(uc, &GPRs[0], &GPRPointers[0], GPRs.size());
    SetGS(threadstate, parenthread->CPUState.gs);
    SetFS(threadstate, parenthread->CPUState.fs);
  }

  // tracing all instructions in the range [EntryPoint, EntryPoint+20]
 // uc_hook_add(uc, &hooks.emplace_back(), UC_HOOK_CODE, (void*)hook_code64, nullptr, 1, 0);

  uc_hook_add(threadstate->uc, &threadstate->hooks.emplace_back(), UC_HOOK_MEM_UNMAPPED, (void *)hook_unmapped, threadstate, 1,
              0);

  // Kick off the execution thread
  threadstate->ExecutionThread = std::thread(&CPUCore::ExecutionThread, this, threadstate);
  return threadstate;
}

void CPUCore::ExecutionThread(ThreadState *Thread) {
  printf("Spinning up the thread\n");
  TLSThread = Thread;

  uint64_t TID = Thread->threadmanager.GetTID();

  std::unique_lock<std::mutex> lk(Thread->StartRunningMutex);
  Thread->StartRunning.wait(lk, [&Thread]{ return Thread->ShouldStart.load(); });
  while (!StopRunning.load() && !Thread->StopRunning.load()) {
//   if (TID != 1)
//     printf(">>> %ld: RIP: 0x%zx\n", TID, Thread->CPUState.rip);

    if (0) {
      printf("\tRIP = 0x%zx\n", Thread->CPUState.rip);
//      printf("\tEFLAGS = 0x%zx\n", Thread->CPUState.rflags);
//      for (int i = 0; i < 16; ++i) {
//        printf("\tgreg[%d] = 0x%zx\n", i, Thread->CPUState.gregs[i]);
//      }
    }


    auto it = Thread->blockcache.FindBlock(Thread->CPUState.rip);
    if (it == Thread->blockcache.End()) {
      it = CompileBlock(Thread).first;
    }

    if (it != Thread->blockcache.End()) {
      // Holy crap, the block actually compiled? Run it!
      using BlockFn = void (*)(CPUCore *cpu);
      BlockFn Ptr;
      Ptr = (BlockFn)it->second;
      Ptr(this);
    }
    else {
 //     printf("%ld fallback to unicorn\n", Thread->threadmanager.GetTID());
      FallbackToUnicorn(Thread);
    }
    if (Thread->CPUState.rip == 0) {
      printf("%ld Hit zero\n", Thread->threadmanager.GetTID());
      if (Thread->threadmanager.GetTID() == 1) {
        StopRunning = true;
      }
      break;
    }

    if (SizesDidNotMatch)
      printf("Instruction sizes didn't match!\n");

    if (::StopRunning)
      break;
    if (PauseThreads.load()) {
      NumThreadsPaused++;
      while(PauseThreads.load());
      NumThreadsPaused--;
    }
  }
}

std::pair<BlockCache::BlockCacheIter, bool> CPUCore::CompileBlock(ThreadState *Thread) {
  void *CodePtr {nullptr};
  uint64_t GuestRIP = Thread->CPUState.rip;

  uint8_t const *Code = MemoryMapper->GetPointer<uint8_t const*>(GuestRIP);
  uint64_t TotalInstructions = 0;
  uint64_t TotalInstructionsLength = 0;
  static uint64_t MaxTotalInstructions = 0;
  bool Done = false;
  uint32_t MAXSIZE = ~0;
  bool HitRIPSetter = false;

  // Do we already have this in the IR cache?
  auto IR = Thread->irlists.find(GuestRIP);
  Emu::IR::IntrusiveIRList *IRList{nullptr};
  if (IR == Thread->irlists.end()) {
    Thread->OpDispatcher.BeginBlock();
    while (!Done) {
      bool HadDispatchError = false;
      auto Info = X86Tables::GetInstInfo(&Code[TotalInstructionsLength]);
      if (!Info.first) {
        printf("Unknown instruction encoding! 0x%zx\n", GuestRIP + TotalInstructionsLength);
        StopRunning = true;
        Thread->OpDispatcher.ResetWorkingList();
        return std::make_pair(Thread->blockcache.End(), false);
      }

      LastInstSize = Info.second.Size;
      Thread->JITRIP = GuestRIP + TotalInstructionsLength;
      if (Info.second.Flags & X86Tables::DECODE_FLAG_LOCK) {
        HadDispatchError = true;
      }
      else if (Info.first->OpcodeDispatcher) {
        Thread->OpDispatcher.AddRIPMarker(GuestRIP + TotalInstructionsLength);
        auto Fn = Info.first->OpcodeDispatcher;
        std::invoke(Fn, Thread->OpDispatcher, Info, &Code[TotalInstructionsLength]);
        if (Thread->OpDispatcher.HadDecodeFailure()) {
//          printf("Decode failure at 0x%zx\n", GuestRIP + TotalInstructionsLength);
          HadDispatchError = true;
        }
        else {
          TotalInstructionsLength += Info.second.Size;
          TotalInstructions++;
        }
      }
      else {
        HadDispatchError = true;
      }

      if (HadDispatchError) {
        if (TotalInstructions == 0) {
          // Couldn't handle any instruction in op dispatcher
          Thread->OpDispatcher.ResetWorkingList();
          return std::make_pair(Thread->blockcache.End(), false);
        }
        else {
          // We had some instructions. Early exit
          Done = true;
        }
      }
      if (Info.first->Flags & X86Tables::FLAGS_BLOCK_END) {
        Done = true;
      }
      if (!HadDispatchError && (Info.first->Flags & X86Tables::FLAGS_SETS_RIP)) {
        Done = true;
        HitRIPSetter = true;
      }

      if (TotalInstructions >= MAXSIZE) {
        Done = true;
      }
    }

    Thread->OpDispatcher.EndBlock(HitRIPSetter ? 0 : TotalInstructionsLength);

    auto IR = Thread->irlists.emplace(std::make_pair(GuestRIP, Thread->OpDispatcher.GetWorkingIR()));
    Thread->OpDispatcher.ResetWorkingList();

    // XXX: Analysis
    AnalysisPasses.FunctionManager.Run();
    AnalysisPasses.BlockManager.Run();

    // XXX: Optimization
    OptimizationPasses.FunctionManager.Run();
    OptimizationPasses.BlockManager.Run();
    // XXX: Code Emission
    IRList = &IR.first->second;
  }
  else {
    IRList = &IR->second;
  }

  if (GuestRIP >= 0x402350 && GuestRIP < 0x4023bc) {
    printf("Created block of %ld instructions from 0x%lx\n", TotalInstructions, GuestRIP);
    MaxTotalInstructions = TotalInstructions;
  }

  if (GuestRIP == 0x402350) {
    IRList->Dump();
  }

  CodePtr = Backend->CompileCode(IRList);
	if (CodePtr)
		return std::make_pair(Thread->blockcache.AddBlockMapping(GuestRIP, CodePtr), true);
	else
		return std::make_pair(Thread->blockcache.End(), false);
}

void CPUCore::FallbackToUnicorn(ThreadState *Thread) {
  std::array<int, 34> GPRs = {
    UC_X86_REG_RIP,
    UC_X86_REG_RAX,
    UC_X86_REG_RBX,
    UC_X86_REG_RCX,
    UC_X86_REG_RDX,
    UC_X86_REG_RSI,
    UC_X86_REG_RDI,
    UC_X86_REG_RBP,
    UC_X86_REG_RSP,
    UC_X86_REG_R8,
    UC_X86_REG_R9,
    UC_X86_REG_R10,
    UC_X86_REG_R11,
    UC_X86_REG_R12,
    UC_X86_REG_R13,
    UC_X86_REG_R14,
    UC_X86_REG_R15,
    UC_X86_REG_XMM0,
    UC_X86_REG_XMM1,
    UC_X86_REG_XMM2,
    UC_X86_REG_XMM3,
    UC_X86_REG_XMM4,
    UC_X86_REG_XMM5,
    UC_X86_REG_XMM6,
    UC_X86_REG_XMM7,
    UC_X86_REG_XMM8,
    UC_X86_REG_XMM9,
    UC_X86_REG_XMM10,
    UC_X86_REG_XMM11,
    UC_X86_REG_XMM12,
    UC_X86_REG_XMM13,
    UC_X86_REG_XMM14,
    UC_X86_REG_XMM15,
    UC_X86_REG_EFLAGS,
  };
  std::array<void*, GPRs.size()> GPRPointers = {
    &Thread->CPUState.rip,
    &Thread->CPUState.gregs[0],
    &Thread->CPUState.gregs[1],
    &Thread->CPUState.gregs[2],
    &Thread->CPUState.gregs[3],
    &Thread->CPUState.gregs[4],
    &Thread->CPUState.gregs[5],
    &Thread->CPUState.gregs[6],
    &Thread->CPUState.gregs[7],
    &Thread->CPUState.gregs[8],
    &Thread->CPUState.gregs[9],
    &Thread->CPUState.gregs[10],
    &Thread->CPUState.gregs[11],
    &Thread->CPUState.gregs[12],
    &Thread->CPUState.gregs[13],
    &Thread->CPUState.gregs[14],
    &Thread->CPUState.gregs[15],
    &Thread->CPUState.xmm[0],
    &Thread->CPUState.xmm[1],
    &Thread->CPUState.xmm[2],
    &Thread->CPUState.xmm[3],
    &Thread->CPUState.xmm[4],
    &Thread->CPUState.xmm[5],
    &Thread->CPUState.xmm[6],
    &Thread->CPUState.xmm[7],
    &Thread->CPUState.xmm[8],
    &Thread->CPUState.xmm[9],
    &Thread->CPUState.xmm[10],
    &Thread->CPUState.xmm[11],
    &Thread->CPUState.xmm[12],
    &Thread->CPUState.xmm[13],
    &Thread->CPUState.xmm[14],
    &Thread->CPUState.xmm[15],
    &Thread->CPUState.rflags,
  };
  static_assert(GPRs.size() == GPRPointers.size());

  auto SetUnicornRegisters = [&]() {
    uc_reg_write_batch(Thread->uc, &GPRs[0], &GPRPointers[0], GPRs.size());
    SetGS(Thread);
    SetFS(Thread);
  };

  auto LoadUnicornRegisters = [&]() {
    uc_reg_read_batch(Thread->uc, &GPRs[0], &GPRPointers[0], GPRs.size());
    {
      uc_x86_msr Val;
      Val.rid = 0xC0000101;
      Val.value = 0;
      uc_reg_read(Thread->uc, UC_X86_REG_MSR, &Val);
      Thread->CPUState.gs = Val.value;
    }
    {
      uc_x86_msr Val;
      Val.rid = 0xC0000100;
      Val.value = 0;
      uc_reg_read(Thread->uc, UC_X86_REG_MSR, &Val);
      Thread->CPUState.fs = Val.value;
    }
  };

  SetUnicornRegisters();
  uc_err err = uc_emu_start(Thread->uc, Thread->CPUState.rip, 0, 0, 1);
  if (err) {
    printf("Failed on uc_emu_start() with error returned %u: %s\n", err,
           uc_strerror(err));
    StopRunning = true;
  }

  LoadUnicornRegisters();
}
}
