#include "Bootloader/Bootloader.h"
#include "Core.h"
#include "ELFLoader.h"
#include "LogManager.h"
#include <unicorn/unicorn.h>
#include <unicorn/x86.h>

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

namespace Emu {

Core::Core()
  : CPU{&MemoryMapper} {
}

bool Core::Load(std::string const &File, std::vector<std::string> const &Args) {
  bool Result = true;
  // Allocate a 64GB SHM region for fun
  Result &= MemoryMapper.AllocateSHMRegion(1ULL << 33);

  ::ELFLoader::ELFContainer file(File);

  //// Allocate 4GB of virtual memory for fun
  //size_t Size = 0x1'0000'0000;
  //void *Ptr = MemoryMapper.MapRegion(0, Size);

  uc_engine *uc;
  uc_err err;

  //err = uc_mem_map_ptr(uc, 0, Size, UC_PROT_ALL, Ptr);
  //LogMan::Throw::A(!err, "Failed Map");

  //Result &= BL.Load(File, Args);
  auto MemLayout = file.GetLayout();
  printf("Emulate x86_64 code\n");

  // Initialize emulator in X86-64bit mode
  err = uc_open(UC_ARCH_X86, UC_MODE_64, &uc);
  LogMan::Throw::A(!err, "Failed on uc_open()");

  {
    uint64_t BasePtr = AlignDown(std::get<0>(MemLayout), PAGE_SIZE);
    uint64_t BaseSize = AlignUp(std::get<2>(MemLayout), PAGE_SIZE);

    void *Ptr = MemoryMapper.MapRegion(BasePtr, BaseSize);
    err = uc_mem_map_ptr(uc, BasePtr, BaseSize, UC_PROT_ALL, Ptr);
    LogMan::Throw::A(!err, "Failed Map");
  }

  {
    void *Ptr = MemoryMapper.MapRegion(STACK_OFFSET, STACK_SIZE);
    err = uc_mem_map_ptr(uc, STACK_OFFSET, STACK_SIZE, UC_PROT_ALL, Ptr);
    LogMan::Throw::A(!err, "Failed Stack Map");
  }

  {
    void *Ptr = MemoryMapper.MapRegion(FS_OFFSET, 0x1000);
    err = uc_mem_map_ptr(uc, FS_OFFSET, 0x1000, UC_PROT_WRITE | UC_PROT_READ, Ptr);
    LogMan::Throw::A(!err, "Failed FS Map");
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

  CPU.uc = uc;
  CPU.CPUState.gregs[REG_RSP] = rsp;
  CPU.CPUState.rip = file.GetEntryPoint();
  CPU.Init();
  CPU.RunLoop();

  return Result;
}

}
