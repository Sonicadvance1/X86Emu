#include "ELFLoader.h"
#include "LogManager.h"
#include <cstring>
#include <stdio.h>
#include <sys/stat.h>
#include <unicorn/unicorn.h>
#include <unicorn/x86.h>

uint64_t AlignUp(uint64_t value, uint64_t size) {
  return value + (size - value % size) % size;
};
uint64_t AlignDown(uint64_t value, uint64_t size) {
  return value - value % size;
};
constexpr uint64_t PAGE_SIZE = 4096;
constexpr uint64_t FS_OFFSET = 0xb0000000;

void MsgHandler(LogMan::DebugLevels Level, std::string const &Message) {
  const char *CharLevel{nullptr};

  switch (Level) {
  case LogMan::NONE:
    CharLevel = "NONE";
    break;
  case LogMan::ASSERT:
    CharLevel = "ASSERT";
    break;
  case LogMan::ERROR:
    CharLevel = "ERROR";
    break;
  case LogMan::DEBUG:
    CharLevel = "DEBUG";
    break;
  case LogMan::INFO:
    CharLevel = "Info";
    break;
  default:
    CharLevel = "???";
    break;
  }
  printf("[%s] %s\n", CharLevel, Message.c_str());
}

void ErrorHandler(std::string const &Message) {
  printf("[ERR] %s\n", Message.c_str());
}

static void hook_block(uc_engine *uc, uint64_t address, uint32_t size,
                       void *user_data) {
  ELFLoader::ELFContainer *file = reinterpret_cast<ELFLoader::ELFContainer*>(user_data);
  auto Sym = file->GetSymbolInRange(std::make_pair(address, address));
  std::string Name = "???";
  if (Sym)
    Name = Sym->Name;
  printf(">>> Tracing basic block at 0x%zx('%s'), block size = 0x%x\n", address,
      Name.c_str(),
      size);

}

static void hook_code64(uc_engine *uc, uint64_t address, uint32_t size,
                        void *user_data) {

  ELFLoader::ELFContainer *file = reinterpret_cast<ELFLoader::ELFContainer*>(user_data);
  uint64_t rip;

  uc_reg_read(uc, UC_X86_REG_RIP, &rip);
  printf(">>> Tracing instruction at 0x%" PRIx64 ", instruction size = 0x%x\n",
         address, size);

  auto Sym = file->GetSymbol(rip);
  if (!Sym)
    Sym = file->GetSymbolInRange(std::make_pair(rip, rip + size));
  std::string Name = "???";
  if (Sym)
    Name = Sym->Name;
  printf(">>> RIP is 0x%" PRIx64 " %s\n", rip, Name.c_str());
}

static void hook_unmapped(uc_engine *uc, uc_mem_type type, uint64_t address,
                          int size, int64_t value, void *user_data) {
  uint64_t rip;

  uc_reg_read(uc, UC_X86_REG_RIP, &rip);
  printf(">>> RIP is 0x%" PRIx64 "\n", rip);

  printf("Attempted to access 0x%zx with type %d, size 0x%08x\n", address, type,
         size);
}

static void hook_printf(uc_engine *uc, uint64_t address, uint32_t size,
                        void *user_data) {
  char Txt[512];
  memset(Txt, 0, sizeof(Txt));

  uint64_t CurrentArgument = 1;
  uint64_t CurrentFloatArgument = 0;
  std::array<uc_x86_reg, 6> ArgumentOrdering = {
      UC_X86_REG_RDI, UC_X86_REG_RSI, UC_X86_REG_RDX,
      UC_X86_REG_RCX, UC_X86_REG_R8D, UC_X86_REG_R9D,
  };
  std::array<uc_x86_reg, 8> FloatArgumentOrdering = {
      UC_X86_REG_XMM0, UC_X86_REG_XMM1, UC_X86_REG_XMM2, UC_X86_REG_XMM3,
      UC_X86_REG_XMM4, UC_X86_REG_XMM5, UC_X86_REG_XMM6,
  };
  if (0) {
    int64_t rax = 0xDEADBEEFBAD0DAD1;
    int64_t rbx = 0xDEADBEEFBAD0DAD1;
    int64_t rcx = 0xDEADBEEFBAD0DAD1;
    int64_t rdx = 0xDEADBEEFBAD0DAD1;
    int64_t rsi = 0xDEADBEEFBAD0DAD1;
    int64_t rdi = 0xDEADBEEFBAD0DAD1;
    int64_t r8 = 0xDEADBEEFBAD0DAD1;
    int64_t r9 = 0xDEADBEEFBAD0DAD1;
    int64_t r10 = 0xDEADBEEFBAD0DAD1;
    int64_t r11 = 0xDEADBEEFBAD0DAD1;
    int64_t r12 = 0xDEADBEEFBAD0DAD1;
    int64_t r13 = 0xDEADBEEFBAD0DAD1;
    int64_t r14 = 0xDEADBEEFBAD0DAD1;
    int64_t r15 = 0xDEADBEEFBAD0DAD1;

    int64_t rsp = 0x200000;

    uc_reg_read(uc, UC_X86_REG_RAX, &rax);
    uc_reg_read(uc, UC_X86_REG_RBX, &rbx);
    uc_reg_read(uc, UC_X86_REG_RCX, &rcx);
    uc_reg_read(uc, UC_X86_REG_RDX, &rdx);
    uc_reg_read(uc, UC_X86_REG_RSI, &rsi);
    uc_reg_read(uc, UC_X86_REG_RDI, &rdi);
    uc_reg_read(uc, UC_X86_REG_R8, &r8);
    uc_reg_read(uc, UC_X86_REG_R9, &r9);
    uc_reg_read(uc, UC_X86_REG_R10, &r10);
    uc_reg_read(uc, UC_X86_REG_R11, &r11);
    uc_reg_read(uc, UC_X86_REG_R12, &r12);
    uc_reg_read(uc, UC_X86_REG_R13, &r13);
    uc_reg_read(uc, UC_X86_REG_R14, &r14);
    uc_reg_read(uc, UC_X86_REG_R15, &r15);
    uc_reg_read(uc, UC_X86_REG_RSP, &rsp);

    printf(">>> RAX = 0x%" PRIx64 "\n", rax);
    printf(">>> RBX = 0x%" PRIx64 "\n", rbx);
    printf(">>> RCX = 0x%" PRIx64 "\n", rcx);
    printf(">>> RDX = 0x%" PRIx64 "\n", rdx);
    printf(">>> RSI = 0x%" PRIx64 "\n", rsi);
    printf(">>> RDI = 0x%" PRIx64 "\n", rdi);
    printf(">>> R8 = 0x%" PRIx64 "\n", r8);
    printf(">>> R9 = 0x%" PRIx64 "\n", r9);
    printf(">>> R10 = 0x%" PRIx64 "\n", r10);
    printf(">>> R11 = 0x%" PRIx64 "\n", r11);
    printf(">>> R12 = 0x%" PRIx64 "\n", r12);
    printf(">>> R13 = 0x%" PRIx64 "\n", r13);
    printf(">>> R14 = 0x%" PRIx64 "\n", r14);
    printf(">>> R15 = 0x%" PRIx64 "\n", r15);
    printf(">>> RSP = 0x%" PRIx64 "\n", rsp);
  }
  uint64_t rdi;
  uc_reg_read(uc, UC_X86_REG_RDI, &rdi);

  uc_mem_read(uc, rdi, &Txt, sizeof(Txt));

  size_t StringSize = std::min(strlen(Txt), 512ul);

  std::ostringstream outString;
  for (size_t i = 0; i < StringSize; ++i) {
    if (Txt[i] == '%') {
      std::string Argument = "%";
      ++i;
      if (Txt[i] == '%') {
        outString << Txt[i];
      }

      if (i >= StringSize)
        break;

      Argument += Txt[i];
      switch (Txt[i]) {
      case 's':
        if (CurrentArgument > ArgumentOrdering.size()) {
          outString << "STACK";
        } else {
          char TmpTxt[512];
          uint64_t reg;
          memset(TmpTxt, 0, sizeof(TmpTxt));

          uc_reg_read(uc, ArgumentOrdering[CurrentArgument], &reg);
          uc_mem_read(uc, reg, &TmpTxt, sizeof(TmpTxt));

          outString << TmpTxt;
          CurrentArgument++;
        }
        break;
      case 'd':
        int64_t reg;
        uc_reg_read(uc, ArgumentOrdering[CurrentArgument], &reg);
        outString << reg;
        CurrentArgument++;
        break;
      case 'p': {
        uint64_t reg;

        uc_reg_read(uc, ArgumentOrdering[CurrentArgument], &reg);

        outString << std::hex << reg;
        CurrentArgument++;
      } break;
      default:
        outString << "???(" << Txt[i] << ")";
      }
    } else {
      outString << Txt[i];
    }
  }

  printf("%s%s", outString.str().c_str(), user_data ? "\n" : "");

  // Let's return early to not have to do the emulated printf
  if (0) {
    uint64_t rsp;
    uint64_t rip;
    uc_reg_read(uc, UC_X86_REG_RSP, &rsp);
    uc_mem_read(uc, rsp, &rip, sizeof(rip));
    rsp += 8;
    uc_reg_write(uc, UC_X86_REG_RIP, &rip);
  }
  // uc_emu_stop(uc);
}
static struct uc_struct *CurrentUC;
class FD {
public:
  FD(int fd, const char *pathname, int flags, mode_t mode)
    : FDesc{fd}
    , Name{pathname}
    , Flags{flags}
    , Mode{mode} {
  }

  ssize_t writev(int fd, uint64_t iov, int iovcnt) {
    ssize_t FinalSize = 0;
    printf("writev: %d 0x%zx %d\n", fd, iov, iovcnt);
    for (int i = 0; i < iovcnt; ++i) {
      struct {
        uint64_t iov_base;
        size_t iov_len;
      } iovObject;
      uc_mem_read(CurrentUC, iov, &iovObject, sizeof(iovObject));
      iov += sizeof(iovObject);
      std::vector<uint8_t> Tmp;
      Tmp.resize(iovObject.iov_len);
      uc_mem_read(CurrentUC, iovObject.iov_base, &Tmp.at(0), iovObject.iov_len);
      printf("\t 0x%zx Size: 0x%zx\n", iovObject.iov_base, iovObject.iov_len);
      printf("\t %s\n", &Tmp.at(0));
      FinalSize += iovObject.iov_len;

    }
    return FinalSize;
  }
  int FDesc{0};
  std::string Name;
  int Flags;
  mode_t Mode;
};

class FileMapper {
public:
  ssize_t writev(int fd, uint64_t iov, int iovcnt) {
    auto fdp = FDMap.find(fd);
    if (fdp == FDMap.end())
      return -1;
    return fdp->second->writev(fd, iov, iovcnt);
  }

  int openat(int dirfd, const char *pathname, int flags, mode_t mode) {
    int32_t fdNum = CurrentFD;
    printf("Opened file: %s with fd: %d\n", pathname, fdNum);
    FD FDObject{fdNum, pathname, flags, mode};
    auto fd = &FDs.emplace_back(FDObject);
    FDMap[CurrentFD] = fd;
    CurrentFD++;
    return fdNum;
  }

  int32_t CurrentFD{0};
  std::vector<FD> FDs;
  std::unordered_map<int32_t, FD*> FDMap;
};

static uint64_t DataSpace = 0;
static uint64_t DataSpaceSize = 0;
static FileMapper FDMapper;

static void hook_syscall(struct uc_struct *uc, void *user_data) {
  CurrentUC = uc;
  uint64_t rip, rax, rdi, rsi, rdx, r10, r8, r9;

  uc_reg_read(uc, UC_X86_REG_RIP, &rip);
  uc_reg_read(uc, UC_X86_REG_RAX, &rax);
  uc_reg_read(uc, UC_X86_REG_RDI, &rdi);
  uc_reg_read(uc, UC_X86_REG_RSI, &rsi);
  uc_reg_read(uc, UC_X86_REG_RDX, &rdx);
  uc_reg_read(uc, UC_X86_REG_R10, &r10);
  uc_reg_read(uc, UC_X86_REG_R8, &r8);
  uc_reg_read(uc, UC_X86_REG_R9, &r9);

  // The kernel interface uses %rdi, %rsi, %rdx, %r10, %r8 and %r9.
  printf(">>> SYSCALL RIP is 0x%zx, SYSCALL is %zd\n", rip, rax);

  switch (rax) {
  default:
    rax = -1;
    LogMan::Msg::E("Unknown syscall\n");
    uc_emu_stop(uc);
    break;
  case 12 :{ //__NR_brk
    constexpr uint64_t DataOffset = 0xa00000000;

    int64_t memoryBase = rdi;

    if (memoryBase == 0) {
      rax = DataOffset;
      DataSpace = DataOffset;
    }
    else {
      // Unmap anything we could have previously had mapped here
      uc_mem_unmap(uc, DataSpace, AlignUp(DataSpaceSize, PAGE_SIZE)); // We don't care if you fail
      uint64_t addedSize = memoryBase - DataSpace;

      // rax = 0xa... + 0

      DataSpaceSize += addedSize;

      rax = DataSpace + DataSpaceSize;

      printf("Incrementing size by: 0x%zx\n", addedSize);
      uc_err err = uc_mem_map(uc, DataSpace, AlignUp(DataSpaceSize, PAGE_SIZE), UC_PROT_ALL);
      if (err) {
        printf("Failed on uc_mem_map() with error returned %u: %s\n", err,
            uc_strerror(err));
      }

      LogMan::Throw::A(!err, "Failed Stack Map");

    }
  }
  break;
  case 0: // __NR_read
  case 2: // __NR_open
  case 3: // __NR_close
  case 8: // __NR_lseek
  case 13: // __NR_rt_sigaction
  case 14: // __NR_rt_sigprocmask
  case 202: // __NR_futex
    rax = 0;
  break;
  case 1: { // __NR_write
    // rdi = file
    // rsi = buf
    // rdx = bytes
    char tmp[512];
    uc_mem_read(uc, rsi, &tmp, rdx);
    rax = write(rdi, tmp, rdx);
  }
  break;
  case 5: { // __NR_fstat
    // rdi = fd
    // rsi = stat*
    if (rdi == STDOUT_FILENO || rdi == STDERR_FILENO) {
      struct stat tmp;
      rax = fstat(rdi, &tmp);
      uc_mem_write(uc, rsi, &tmp, sizeof(tmp));
    }
    else {
      printf("Attempting to stat: %ld\n", rdi);
      uc_emu_stop(uc);
    }
  }
  break;
  case 9: { // __NR_mmap
    // rdi = addr
    // rsi = len
    // rdx = prot
    // r10 = flags
    // r8 = fd
    // r9 = off
    uc_emu_stop(uc);
  }
  break;
  case 20: { // __NR_writev
    // rdi = fd
    // rsi = iov
    // rdx = iovcnt
    rax = FDMapper.writev(rdi, rsi, rdx);
  break;
  }
  case 21: { // __NR_access
    char tmp[512];
    uc_mem_read(uc, rdi, &tmp, 512);
    printf("Trying to read access of : %s\n", tmp);
    rax = -1;
  break;
  }
  case 89: // __NR_readlink
    rax = -1;
    char tmp[512];
    uc_mem_read(uc, rdi, &tmp, 512);
    printf("Trying to readlink of : %s\n", tmp);
  break;
  case 60: // __NR_exit
    printf("Program exited with: %zd\n", rdi);
    uc_emu_stop(uc);
  break;
  case 63: { // __NR_uname
    // RSI contains pointer to utsname structure
    struct _utsname {
      char sysname[65];
      char nodename[65];
      char release[65];
      char version[65];
      char machine[65];
    };
    _utsname local;
    uc_mem_read(uc, rsi, &local, sizeof(local));

    const char Name[] = "Linux\0";
    const char Version[] = "4.19\0";
    strcpy(local.release, Version);
//    uc_mem_write(uc, local.ptrs[0], Name, sizeof(Name));

    uc_mem_write(uc, rdi, &local, sizeof(local));
    rax = 0;
  //  uc_emu_stop(uc);
  }
  break;
  case 39: // __NR_getpid
    rax = 1;
  break;
  case 102: // __NR_getuid
    rax = 1;
  break;
  case 104: // __NR_getgid
    rax = 1;
  break;
  case 107: //__NR_geteuid
    rax = 1;
  break;
  case 108: // __NR_getegid
    rax = 1;
  break;
  case 158: { // __NR_arch_prctl
    uint64_t option = rdi;
    switch (option) {
    case 0x1002: { // ARCH_SET_FS
    // rdi = option
    // rsi = ptr

//      uc_reg_write(uc, UC_X86_REG_FS, &rsi); // XXX: Is this actually from RSI?

        uc_x86_msr Val;
        Val.rid = 0xC0000100;
        Val.value = rsi;
        uc_reg_write(uc, UC_X86_REG_MSR, &Val);
//      uc_emu_stop(uc);

      rax = 0;
    }
    break;
    default:
      LogMan::Msg::E("Unknown prctl\n");
      uc_emu_stop(uc);
    }
  }
  break;
  case 186: // __NR_gettid
    rax = 1;
  break;
  case 228: { // __NR_clock_gettime
    // rdi = which_clock
    // rsi = tp
    printf("Attempting to get time %d to %zx\n", rdi, rsi);
  //  uc_emu_stop(uc);
    timespec tmp;
    int res = clock_gettime(rdi, &tmp);
    uc_mem_write(uc, rsi, &tmp, sizeof(tmp));
    rax = 0;
  break;
  }
  case 234: // __NR_tgkill
    rax = 0;
  break;
  case 257: { //__NR_openat
    // rdi = dfd = -100
    // rsi = filename = /dev/tty
    // rdx = flags = 0x902 = O_NONBLOCK | O_NOCTTY | O_RDWR
    // r10 = mode = 0

    char tmp[512];
    uc_mem_read(uc, rsi, &tmp, 512);
    printf("Trying to openat of : %s\n", tmp);
    rax = FDMapper.openat(rdi, tmp, rdx, r10);
   // uc_emu_stop(uc);

  break;
  }
  }
  uc_reg_write(uc, UC_X86_REG_RAX, &rax);
}

void TestUnicorn(ELFLoader::ELFContainer &file) {
  uc_engine *uc;
  uc_err err;
  std::vector<uc_hook> hooks;

  int64_t rax = 0xDEADBEEFBAD0DAD1;
  int64_t rbx = 0xDEADBEEFBAD0DAD1;
  int64_t rcx = 0xDEADBEEFBAD0DAD1;
  int64_t rdx = 0xDEADBEEFBAD0DAD1;
  int64_t rsi = 0xDEADBEEFBAD0DAD1;
  int64_t rdi = 0xDEADBEEFBAD0DAD1;
  int64_t r8 = 0xDEADBEEFBAD0DAD1;
  int64_t r9 = 0xDEADBEEFBAD0DAD1;
  int64_t r10 = 0xDEADBEEFBAD0DAD1;
  int64_t r11 = 0xDEADBEEFBAD0DAD1;
  int64_t r12 = 0xDEADBEEFBAD0DAD1;
  int64_t r13 = 0xDEADBEEFBAD0DAD1;
  int64_t r14 = 0xDEADBEEFBAD0DAD1;
  int64_t r15 = 0xDEADBEEFBAD0DAD1;

  int64_t rsp = 0x200000;
  int64_t rip = 0;
  uint64_t fs = 0x0;

  auto MemLayout = file.GetLayout();
  printf("Emulate x86_64 code\n");

  // Initialize emulator in X86-64bit mode
  err = uc_open(UC_ARCH_X86, UC_MODE_64, &uc);
  LogMan::Throw::A(!err, "Failed on uc_open()");

  uint64_t BasePtr = AlignDown(std::get<0>(MemLayout), PAGE_SIZE);
  uint64_t BaseSize = AlignUp(std::get<2>(MemLayout), PAGE_SIZE);
  err = uc_mem_map(uc, BasePtr, BaseSize, UC_PROT_ALL);
  LogMan::Throw::A(!err, "Failed Map");

  uint64_t STACKSIZE = 64 * 1024 * 1024;
  uint64_t STACKOFFSET = AlignDown(0x1000000000 - STACKSIZE, PAGE_SIZE);
  uint64_t SAFEZONE = 0x1000;
  err = uc_mem_map(uc, STACKOFFSET, STACKSIZE, UC_PROT_ALL);
  LogMan::Throw::A(!err, "Failed Stack Map");

  err = uc_mem_map(uc, FS_OFFSET, 0x1000, UC_PROT_WRITE | UC_PROT_READ);
  LogMan::Throw::A(!err, "Failed FS Map");

  rsp = STACKOFFSET + STACKSIZE;

  std::vector<uint8_t> Values = {
      2,    0,   0,   0,   0,   0, 0, 0, // Argument count
      0,    0,   0,   0,   0,   0, 0, 0, // Argument0 pointer
      0,    0,   0,   0,   0,   0, 0, 0, // Argument1 pointer
      'B',  'u', 't', 't', 's',          // Argument0
      '\0',
  };

  rsp -= Values.size() + SAFEZONE;
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

  printf("Setting RSP to 0x%zx (0x%zx - 0x%zx)\n", rsp, STACKOFFSET,
         STACKOFFSET + STACKSIZE);
  printf("Arg0 offset: 0x%zx\n", arg0offset);
  // initialize machine registers
  uc_reg_write(uc, UC_X86_REG_RSP, &rsp);

  uc_reg_write(uc, UC_X86_REG_RAX, &rax);
  uc_reg_write(uc, UC_X86_REG_RBX, &rbx);
  uc_reg_write(uc, UC_X86_REG_RCX, &rcx);
  uc_reg_write(uc, UC_X86_REG_RDX, &rdx);
  uc_reg_write(uc, UC_X86_REG_RSI, &rsi);
  uc_reg_write(uc, UC_X86_REG_RDI, &rdi);
  uc_reg_write(uc, UC_X86_REG_R8, &r8);
  uc_reg_write(uc, UC_X86_REG_R9, &r9);
  uc_reg_write(uc, UC_X86_REG_R10, &r10);
  uc_reg_write(uc, UC_X86_REG_R11, &r11);
  uc_reg_write(uc, UC_X86_REG_R12, &r12);
  uc_reg_write(uc, UC_X86_REG_R13, &r13);
  uc_reg_write(uc, UC_X86_REG_R14, &r14);
  uc_reg_write(uc, UC_X86_REG_R15, &r15);

  uint64_t EntryPoint = file.GetEntryPoint();
  auto EntryPointSymbol = file.GetSymbol(EntryPoint);

//   uc_hook_add(uc, &hooks.emplace_back(), UC_HOOK_BLOCK, (void*)hook_block, &file, 1, 0);

  // tracing all instructions in the range [EntryPoint, EntryPoint+20]
//  uc_hook_add(uc, &hooks.emplace_back(), UC_HOOK_CODE, (void*)hook_code64, &file, 1, 0);

  uc_hook_add(uc, &hooks.emplace_back(), UC_HOOK_MEM_UNMAPPED, (void *)hook_unmapped, NULL, 1,
              0);

  if (auto Sym = file.GetSymbol("printf"))
    uc_hook_add(uc, &hooks.emplace_back(), UC_HOOK_CODE, (void *)hook_printf, NULL,
                Sym->Address, Sym->Address);


  if (auto Sym = file.GetSymbol("puts"))
    uc_hook_add(uc, &hooks.emplace_back(), UC_HOOK_CODE, (void *)hook_printf, (void*)1,
                Sym->Address, Sym->Address);

//  if (auto Sym = file.GetSymbol("_IO_puts"))
//    uc_hook_add(uc, &hooks.emplace_back(), UC_HOOK_CODE, (void *)hook_printf, NULL,
//                Sym->Address, Sym->Address);

  if (auto Sym = file.GetSymbol("_dl_fatal_printf"))
    uc_hook_add(uc, &hooks.emplace_back(), UC_HOOK_CODE, (void *)hook_printf, NULL,
                Sym->Address, Sym->Address);

  uc_hook_add(uc, &hooks.emplace_back(), UC_HOOK_BLOCK, (void *)hook_printf, NULL,
              0x412500, 0x412500);

  err = uc_hook_add(uc, &hooks.emplace_back(), UC_HOOK_INSN, (void *)hook_syscall, NULL, 1, 0, UC_X86_INS_SYSCALL);

  if (err) {
    printf("Failed on hook() with error returned %u: %s\n", err,
           uc_strerror(err));
  }

  printf("Mapped BasePtr:    %016lx\n", BasePtr);
  printf("Mapped to BasePtr: %016lx\n", BasePtr + BaseSize);
  printf("EntryPoint:        %016lx\n", EntryPoint);
  err = uc_emu_start(uc, EntryPoint, 0, 0, 0);
  if (err) {
    printf("Failed on uc_emu_start() with error returned %u: %s\n", err,
           uc_strerror(err));
  }

  // now print out some registers
  printf(">>> Emulation done. Below is the CPU context\n");

  uc_reg_read(uc, UC_X86_REG_RAX, &rax);
  uc_reg_read(uc, UC_X86_REG_RBX, &rbx);
  uc_reg_read(uc, UC_X86_REG_RCX, &rcx);
  uc_reg_read(uc, UC_X86_REG_RDX, &rdx);
  uc_reg_read(uc, UC_X86_REG_RSI, &rsi);
  uc_reg_read(uc, UC_X86_REG_RDI, &rdi);
  uc_reg_read(uc, UC_X86_REG_R8, &r8);
  uc_reg_read(uc, UC_X86_REG_R9, &r9);
  uc_reg_read(uc, UC_X86_REG_R10, &r10);
  uc_reg_read(uc, UC_X86_REG_R11, &r11);
  uc_reg_read(uc, UC_X86_REG_R12, &r12);
  uc_reg_read(uc, UC_X86_REG_R13, &r13);
  uc_reg_read(uc, UC_X86_REG_R14, &r14);
  uc_reg_read(uc, UC_X86_REG_R15, &r15);
  uc_reg_read(uc, UC_X86_REG_RSP, &rsp);
  uc_reg_read(uc, UC_X86_REG_RIP, &rip);
  uc_reg_read(uc, UC_X86_REG_FS, &fs);


  printf(">>> RAX = 0x%" PRIx64 "\n", rax);
  printf(">>> RBX = 0x%" PRIx64 "\n", rbx);
  printf(">>> RCX = 0x%" PRIx64 "\n", rcx);
  printf(">>> RDX = 0x%" PRIx64 "\n", rdx);
  printf(">>> RSI = 0x%" PRIx64 "\n", rsi);
  printf(">>> RDI = 0x%" PRIx64 "\n", rdi);
  printf(">>> R8 = 0x%" PRIx64 "\n", r8);
  printf(">>> R9 = 0x%" PRIx64 "\n", r9);
  printf(">>> R10 = 0x%" PRIx64 "\n", r10);
  printf(">>> R11 = 0x%" PRIx64 "\n", r11);
  printf(">>> R12 = 0x%" PRIx64 "\n", r12);
  printf(">>> R13 = 0x%" PRIx64 "\n", r13);
  printf(">>> R14 = 0x%" PRIx64 "\n", r14);
  printf(">>> R15 = 0x%" PRIx64 "\n", r15);
  printf(">>> RSP = 0x%" PRIx64 "\n", rsp);
  printf(">>> RIP = 0x%" PRIx64 "\n", rip);
  printf(">>> FS = 0x%" PRIx64 "\n", fs);

  uc_close(uc);
}

int main(int argc, char **argv) {
  LogMan::Throw::InstallHandler(ErrorHandler);
  LogMan::Msg::InstallHandler(MsgHandler);

  LogMan::Throw::A(argc > 1, "Not enough arguments");
  ELFLoader::ELFContainer file(argv[1]);

  TestUnicorn(file);
  return 0;
}
