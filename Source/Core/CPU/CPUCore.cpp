#include "CPUCore.h"
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
static void hook_unmapped(uc_engine *uc, uc_mem_type type, uint64_t address,
                          int size, int64_t value, void *user_data) {
  uint64_t rip;

  uc_reg_read(uc, UC_X86_REG_RIP, &rip);
  printf(">>> RIP is 0x%" PRIx64 "\n", rip);

  printf("Attempted to access 0x%zx with type %d, size 0x%08x\n", address, type,
         size);
}

uint64_t LastInstSize = 0;
bool StopRunning = false;
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
void CPUCore::SetGS() {
  uc_x86_msr Val;
  Val.rid = 0xC0000101;

  Val.value = CPUState.gs;
  uc_reg_write(uc, UC_X86_REG_MSR, &Val);
}

void CPUCore::SetFS() {
  uc_x86_msr Val;
  Val.rid = 0xC0000100;
  Val.value = CPUState.fs;
  uc_reg_write(uc, UC_X86_REG_MSR, &Val);
}

void CPUCore::SetGS(uint64_t Value) {
  CPUState.gs = Value;
  SetGS();
}

void CPUCore::SetFS(uint64_t Value) {
  CPUState.fs = Value;
  SetFS();
}

void *CPUCore::MapRegion(uint64_t Offset, uint64_t Size) {
  void *Ptr = MemoryMapper->MapRegion(Offset, Size);

  uc_err err = uc_mem_map_ptr(uc, Offset, Size, UC_PROT_ALL, Ptr);
  if (err) {
    printf("Failed on uc_mem_map() with error returned %u: %s\n", err,
        uc_strerror(err));
  }
  return Ptr;
}

CPUCore::CPUCore(Memmap *Mapper)
  : MemoryMapper{Mapper}
  , syscallhandler {this}
  , OpDispatcher {this} {
  // Initialize default CPU state
  CPUState.rip = ~0ULL;
  for (int i = 0; i < 16; ++i) {
    CPUState.gregs[i] = 0;
  }
  for (int i = 0; i < 16; ++i) {
    CPUState.xmm[i][0] = 0xDEADBEEFULL;
    CPUState.xmm[i][1] = 0xBAD0DAD1ULL;
  }

  CPUState.gs = 0;
  CPUState.fs = 0;
  X86Tables::InitializeInfoTables();
  IR::InstallOpcodeHandlers();
}

void CPUCore::Init() {
  // XXX: Allow runtime selection between cores
#if 0
  Backend.reset(CreateLLVMBackend());
#elif 0
  Backend.reset(new AArch64());
#else
  Backend.reset(new Interpreter());
#endif
  uc_reg_write(uc, UC_X86_REG_RSP, &CPUState.gregs[REG_RSP]);
  uc_reg_write(uc, UC_X86_REG_RIP, &CPUState.rip);

  // tracing all instructions in the range [EntryPoint, EntryPoint+20]
 // uc_hook_add(uc, &hooks.emplace_back(), UC_HOOK_CODE, (void*)hook_code64, nullptr, 1, 0);

  uc_hook_add(uc, &hooks.emplace_back(), UC_HOOK_MEM_UNMAPPED, (void *)hook_unmapped, NULL, 1,
              0);
}

std::pair<BlockCache::BlockCacheIter, bool> CPUCore::CompileBlock() {
  void *CodePtr {nullptr};
  uint64_t GuestRIP = CPUState.rip;
  uint8_t const *GuestCode = MemoryMapper->GetPointer<uint8_t const*>(CPUState.rip);

  uint8_t const *Code = MemoryMapper->GetPointer<uint8_t const*>(CPUState.rip);
  uint64_t TotalInstructions = 0;
  uint64_t TotalInstructionsLength = 0;
  bool Done = false;

  OpDispatcher.BeginBlock();
  while (!Done) {
    bool HadDispatchError = false;
    auto Info = X86Tables::GetInstInfo(&Code[TotalInstructionsLength]);
    if (!Info.first) {
      printf("Unknown instruction encoding!\n");
      StopRunning = true;
      OpDispatcher.ResetWorkingList();
      return std::make_pair(blockcache.End(), false);
    }

    LastInstSize = Info.second.Size;
    if (Info.first->OpcodeDispatcher) {
      auto Fn = Info.first->OpcodeDispatcher;
      std::invoke(Fn, OpDispatcher, Info, &Code[TotalInstructionsLength]);
      if (OpDispatcher.HadDecodeFailure()) {
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
        OpDispatcher.ResetWorkingList();
        return std::make_pair(blockcache.End(), false);
      }
      else {
        // We had some instructions. Early exit
        Done = true;
      }
    }
    if (Info.first->Flags & X86Tables::FLAGS_BLOCK_END) {
      Done = true;
    }
  }

  OpDispatcher.EndBlock(TotalInstructionsLength);

  auto IR = irlists.emplace(std::make_pair(GuestRIP, OpDispatcher.GetWorkingIR()));
  OpDispatcher.ResetWorkingList();

  // XXX: Analysis
  AnalysisPasses.FunctionManager.Run();
  AnalysisPasses.BlockManager.Run();

  // XXX: Optimization
  OptimizationPasses.FunctionManager.Run();
  OptimizationPasses.BlockManager.Run();
  // XXX: Code Emission

  CodePtr = Backend->CompileCode(&IR.first->second);
	if (CodePtr)
		return std::make_pair(blockcache.AddBlockMapping(GuestRIP, CodePtr), true);
	else
		return std::make_pair(blockcache.End(), false);
}

void CPUCore::RunLoop() {
  while (1) {
    //printf(">>> RIP: 0x%zx\n", CPUState.rip);
    auto it = blockcache.FindBlock(CPUState.rip);
    if (it == blockcache.End()) {
      it = CompileBlock().first;
    }

    if (it != blockcache.End()) {
      // Holy crap, the block actually compiled? Run it!
      using BlockFn = void (*)(CPUCore *cpu);
      BlockFn Ptr;
      Ptr = (BlockFn)it->second;
      Ptr(this);
    }
    else {
      FallbackToUnicorn();
    }
    if (CPUState.rip == 0) {
      printf("Hit zero\n");
      StopRunning = true;
    }

    if (SizesDidNotMatch)
      printf("Instruction sizes didn't match!\n");

    if (StopRunning || ::StopRunning)
      break;
  }
}

void CPUCore::FallbackToUnicorn() {
  std::array<int, 33> GPRs = {
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
  };
  std::array<void*, GPRs.size()> GPRPointers = {
    &CPUState.rip,
    &CPUState.gregs[0],
    &CPUState.gregs[1],
    &CPUState.gregs[2],
    &CPUState.gregs[3],
    &CPUState.gregs[4],
    &CPUState.gregs[5],
    &CPUState.gregs[6],
    &CPUState.gregs[7],
    &CPUState.gregs[8],
    &CPUState.gregs[9],
    &CPUState.gregs[10],
    &CPUState.gregs[11],
    &CPUState.gregs[12],
    &CPUState.gregs[13],
    &CPUState.gregs[14],
    &CPUState.gregs[15],
    &CPUState.xmm[0],
    &CPUState.xmm[1],
    &CPUState.xmm[2],
    &CPUState.xmm[3],
    &CPUState.xmm[4],
    &CPUState.xmm[5],
    &CPUState.xmm[6],
    &CPUState.xmm[7],
    &CPUState.xmm[8],
    &CPUState.xmm[9],
    &CPUState.xmm[10],
    &CPUState.xmm[11],
    &CPUState.xmm[12],
    &CPUState.xmm[13],
    &CPUState.xmm[14],
    &CPUState.xmm[15],
  };
  static_assert(GPRs.size() == GPRPointers.size());

  auto SetUnicornRegisters = [&]() {
    uc_reg_write_batch(uc, &GPRs[0], &GPRPointers[0], GPRs.size());
    SetGS();
    SetFS();
  };

  auto LoadUnicornRegisters = [&]() {
    uc_reg_read_batch(uc, &GPRs[0], &GPRPointers[0], GPRs.size());
    {
      uc_x86_msr Val;
      Val.rid = 0xC0000101;
      Val.value = 0;
      uc_reg_read(uc, UC_X86_REG_MSR, &Val);
      CPUState.gs = Val.value;
    }
    {
      uc_x86_msr Val;
      Val.rid = 0xC0000100;
      Val.value = 0;
      uc_reg_read(uc, UC_X86_REG_MSR, &Val);
      CPUState.fs = Val.value;
    }
  };

  SetUnicornRegisters();
  uc_err err = uc_emu_start(uc, CPUState.rip, 0, 0, 1);
  if (err) {
    printf("Failed on uc_emu_start() with error returned %u: %s\n", err,
           uc_strerror(err));
    StopRunning = true;
  }

  LoadUnicornRegisters();
}
}
