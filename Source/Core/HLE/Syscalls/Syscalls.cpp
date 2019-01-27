#include "Core/CPU/CPUCore.h"
#include <sys/mman.h>
#include "LogManager.h"
#include "Syscalls.h"
#include <cstring>
#include <tuple>

static uint64_t AlignUp(uint64_t value, uint64_t size) {
  return value + (size - value % size) % size;
};
static uint64_t AlignDown(uint64_t value, uint64_t size) {
  return value - value % size;
};

constexpr uint64_t PAGE_SIZE = 4096;

namespace Emu {

uint64_t SyscallHandler::HandleSyscall(SyscallArguments *Args) {
  uint64_t Result = 0;

  uint64_t TID = cpu->GetTLSThread()->threadmanager.GetTID();
  printf("%ld: syscall: %ld\n",cpu->GetTLSThread()->threadmanager.GetTID(), Args->Argument[0]);
  switch (Args->Argument[0]) {
  default:
    Result = -1;
    LogMan::Msg::A("Unknown syscall: ", Args->Argument[0]);
  break;
  // ID Management
  case SYSCALL_GETUID:
    Result = cpu->GetTLSThread()->threadmanager.GetUID();
    printf("UID: 0x%zx\n", Result);
  break;
  case SYSCALL_GETGID:
    Result = cpu->GetTLSThread()->threadmanager.GetGID();
    printf("GID: 0x%zx\n", Result);
  break;
  case SYSCALL_GETEUID:
    Result = cpu->GetTLSThread()->threadmanager.GetEUID();
    printf("EUID: 0x%zx\n", Result);
  break;
  case SYSCALL_GETEGID:
    Result = cpu->GetTLSThread()->threadmanager.GetEGID();
    printf("EGID: 0x%zx\n", Result);
  break;
  case SYSCALL_GETTID:
    Result = cpu->GetTLSThread()->threadmanager.GetTID();
    printf("TID: 0x%zx\n", Result);
  break;
  case SYSCALL_GETPID:
    Result = cpu->GetTLSThread()->threadmanager.GetPID();
    printf("PID: 0x%zx\n", Result);
  break;

  // File Management
  case SYSCALL_READ:
    Result = filemanager.Read(Args->Argument[1], cpu->MemoryMapper->GetPointer(Args->Argument[2]), Args->Argument[3]);
  break;
  case SYSCALL_WRITE:
    Result = filemanager.Write(Args->Argument[1], cpu->MemoryMapper->GetPointer(Args->Argument[2]), Args->Argument[3]);
  break;
  case SYSCALL_OPEN:
    Result = filemanager.Open(cpu->MemoryMapper->GetPointer<const char*>(Args->Argument[1]), Args->Argument[2], Args->Argument[3]);
  break;
  case SYSCALL_CLOSE:
    Result = filemanager.Close(Args->Argument[1]);
  break;
  case SYSCALL_FSTAT:
    Result = filemanager.Fstat(Args->Argument[1], cpu->MemoryMapper->GetPointer(Args->Argument[2]));
  break;
  case SYSCALL_LSEEK:
    Result = filemanager.Lseek(Args->Argument[1], Args->Argument[2], Args->Argument[3]);
  break;
  case SYSCALL_WRITEV:
    Result = filemanager.Writev(Args->Argument[1], cpu->MemoryMapper->GetPointer(Args->Argument[2]), Args->Argument[3]);
  break;
  case SYSCALL_ACCESS:
    Result = filemanager.Access(cpu->MemoryMapper->GetPointer<const char*>(Args->Argument[1]), Args->Argument[2]);
  break;
  case SYSCALL_READLINK:
    Result = filemanager.Readlink(cpu->MemoryMapper->GetPointer<const char*>(Args->Argument[1]), cpu->MemoryMapper->GetPointer<char*>(Args->Argument[2]), Args->Argument[3]);
  break;
  case SYSCALL_OPENAT:
    Result = filemanager.Openat(Args->Argument[1], cpu->MemoryMapper->GetPointer<const char*>(Args->Argument[2]), Args->Argument[3], Args->Argument[4]);
  break;
  case SYSCALL_UNAME: {
    struct _utsname {
      char sysname[65];
      char nodename[65];
      char release[65];
      char version[65];
      char machine[65];
    };
    _utsname *local = cpu->MemoryMapper->GetPointer<_utsname*>(Args->Argument[1]);

    strcpy(local->sysname, "Linux");
    strcpy(local->nodename, "Emu");
    strcpy(local->release, "4.19");
    strcpy(local->version, "#1");
    strcpy(local->machine, "x86_64");
    Result = 0;
  break;
  }

  case SYSCALL_EXIT:
    printf("Thread exited with: %zd\n", Args->Argument[1]);
    cpu->GetTLSThread()->StopRunning = true;
    if (cpu->GetTLSThread()->threadmanager.child_tid) {
      // If we are participating in child_tid then we must clear some things
      uint64_t *tidptr = cpu->MemoryMapper->GetPointer<uint64_t*>(cpu->GetTLSThread()->threadmanager.child_tid);
      *tidptr = 0;
      // Also wake up the futex for this
      Futex *futex = GetFutex(cpu->GetTLSThread()->threadmanager.child_tid);
      if (futex) {
        futex->cv.notify_one();
      }
    }
  break;
  case SYSCALL_ARCH_PRCTL:
    switch (Args->Argument[1]) {
    case 0x1002: // ARCH_SET_FS
      cpu->SetFS(cpu->GetTLSThread(), Args->Argument[2]);
    break;
    default:
      LogMan::Msg::E("Unknown prctl\n");
      cpu->StopRunning = true;
    break;
    }
    Result = 0;
  break;
  case SYSCALL_CLOCK_GETTIME: {
    timespec *res = cpu->MemoryMapper->GetPointer<timespec*>(Args->Argument[2]);
    Result = clock_gettime(Args->Argument[1], res);
  break;
  }

  // XXX: Improve BRK handling
  case SYSCALL_BRK: {
    constexpr uint64_t DataOffset = 0xa000'0000;

    if (Args->Argument[1] == 0) {
      // Just map a GB of space in, if they need more than that in the memory base then screw you
      cpu->MapRegion(cpu->GetTLSThread(), DataOffset, 0x10000000);
      dataspace = DataOffset;
      Result = DataOffset;
    }
    else {
      uint64_t addedSize = Args->Argument[1] - dataspace;

      dataspacesize += addedSize;

      printf("Adding Size: %ld to Space 0x%zx base 0x%lx\n", addedSize, dataspace, Args->Argument[1]);
      Result = dataspace + dataspacesize;
    }
  break;
  }
  case SYSCALL_MMAP: {
    // 0: addr
    // 1: len
    // 2: prot
    // 3: flags
    // 4: fd
    // 5: offset
    static uint64_t LastMMAP = 0xd000'0000;
    uint64_t BasePtr = AlignDown(LastMMAP, PAGE_SIZE);
    uint64_t BaseSize = AlignUp(Args->Argument[2], PAGE_SIZE);
    LastMMAP += BaseSize;
#if 0
    cpu->MapRegion(cpu->GetTLSThread(), BasePtr, BaseSize);
#else
    cpu->MapRegionOnAll(BasePtr, BaseSize);
#endif
    Result = BasePtr;
  break;
  }
  case SYSCALL_CLONE: {
    // 0: clone_flags
    // 1: newsp
    // 2: parent_tidptr
    // 3: child_tidptr
    // 4: tls

  uint64_t *res = cpu->MemoryMapper->GetPointer<uint64_t*>(Args->Argument[2]);

  printf("clone(%lx, %lx, %lx, %lx, %lx)\n",
        Args->Argument[1],
        Args->Argument[2],
        Args->Argument[3],
        Args->Argument[4],
        Args->Argument[5]);
  uint32_t flags = Args->Argument[1];
#define TPRINT(x, y) \
  if (flags & y) printf("\tFlag " #x "\n")
  TPRINT(CSIGNAL		, 0x000000ff);
  TPRINT(CLONE_VM	, 0x00000100);
  TPRINT(CLONE_FS	, 0x00000200);
  TPRINT(CLONE_FILES	, 0x00000401);
  TPRINT(CLONE_SIGHAND	, 0x00801);
  TPRINT(CLONE_PTRACE	, 0x00002001);
  TPRINT(CLONE_VFORK	, 0x00004001);
  TPRINT(CLONE_PARENT	, 0x00008001);
  TPRINT(CLONE_THREAD	, 0x00010001);
  TPRINT(CLONE_NEWNS	, 0x00020001);
  TPRINT(CLONE_SYSVSEM	, 0x00040001);
  TPRINT(CLONE_SETTLS	, 0x00080001);
  TPRINT(CLONE_PARENT_SETTID	, 0x00100001);
  TPRINT(CLONE_CHILD_CLEARTID	, 0x00200001);
  TPRINT(CLONE_DETACHED		, 0x00400001);
  TPRINT(CLONE_UNTRACED		, 0x800001);
  TPRINT(CLONE_CHILD_SETTID	, 0x01000001);
  TPRINT(CLONE_NEWCGROUP		, 0x02000001);
  TPRINT(CLONE_NEWUTS		, 0x04000001);
  TPRINT(CLONE_NEWIPC		, 0x08000001);
  TPRINT(CLONE_NEWUSER		, 0x10000001);
  TPRINT(CLONE_NEWPID		, 0x20000001);
  TPRINT(CLONE_NEWNET		, 0x40000001);
  TPRINT(CLONE_IO		, 0x80000001);

  X86State NewState;
  memcpy(&NewState, &cpu->GetTLSThread()->CPUState, sizeof(X86State));
  NewState.gregs[REG_RAX] = 0;
  NewState.gregs[REG_RSP] = Args->Argument[2];
  NewState.fs = Args->Argument[5];

  // XXX: Hack to offset past the syscall instruction
  NewState.rip += 2;

  auto threadstate = cpu->NewThread(&NewState, Args->Argument[3], Args->Argument[4]);

  // For some reason the kernel does this
  threadstate->CPUState.gregs[REG_RBX] = 0;
  threadstate->CPUState.gregs[REG_RBP] = 0;
  Result = threadstate->threadmanager.GetTID();
    if (flags & CLONE_PARENT_SETTID) {
      uint64_t *tidptr = cpu->MemoryMapper->GetPointer<uint64_t*>(Args->Argument[3]);
      *tidptr = threadstate->threadmanager.GetTID();
    }
  // Time to kick off the thread actually
  std::lock_guard<std::mutex> lk(threadstate->StartRunningMutex);
  threadstate->ShouldStart = true;
  threadstate->StartRunning.notify_all();
  }
  break;
  case SYSCALL_FUTEX: {
    // futex(0x7fa5d28aa9d0, FUTEX_WAIT, 76036, NULL
    // 0: uaddr
    // 1: op
    // 2: val
    // 3: utime
    // 4: uaddr2
    // 5: val3

    uint64_t *res = cpu->MemoryMapper->GetPointer<uint64_t*>(Args->Argument[1]);
    uint8_t Command = Args->Argument[2] & 0x0F;
    printf("%ld: futex(%lx,\n\t%lx,\n\t%lx,\n\t%lx,\n\t%lx,\n\t%lx)\n",
          TID,
          Args->Argument[1],
          Args->Argument[2],
          Args->Argument[3],
          Args->Argument[4],
          Args->Argument[5],
          Args->Argument[6]
          );

    switch (Command) {
    case 0: { // WAIT
      LogMan::Throw::A(!Args->Argument[4], "Can't handle timed futexes");
      Futex *futex = new Futex{};
      futex->addr = cpu->MemoryMapper->GetPointer<std::atomic<uint32_t>*>(Args->Argument[1]);
      futex->val = Args->Argument[3];
      EmplaceFutex(Args->Argument[1], futex);
      std::unique_lock<std::mutex> lk(futex->mute);
      futex->cv.wait(lk, [futex] { return futex->addr->load() != futex->val; });
    }
    break;
    case 1: { // WAKE
      Futex *futex = GetFutex(Args->Argument[1]);
      for (uint32_t i = 0; i < Args->Argument[3]; ++i)
        futex->cv.notify_one();
    }
    break;
    default:
      LogMan::Msg::A("Unknown futex command: ", Command);
    break;
    }
  break;
  }
  case SYSCALL_SET_TID_ADDRESS:
    cpu->GetTLSThread()->threadmanager.child_tid = Args->Argument[1];
    Result = cpu->GetTLSThread()->threadmanager.GetTID();
  break;
  case SYSCALL_SET_ROBUST_LIST:
    cpu->GetTLSThread()->threadmanager.robust_list_head = Args->Argument[1];
    Result = 0;
  break;
  case SYSCALL_NANOSLEEP: {
    const struct timespec *req = cpu->MemoryMapper->GetPointer<const struct timespec *>(Args->Argument[1]);
    struct timespec *rem = cpu->MemoryMapper->GetPointer<struct timespec *>(Args->Argument[2]);
    printf("Time: %ld %ld\n", req->tv_sec, req->tv_nsec);
    Result = nanosleep(req, rem);
  }
  break;
  // XXX: Currently unhandled bit hit
  case SYSCALL_MPROTECT:
  case SYSCALL_RT_SIGACTION:
  case SYSCALL_RT_SIGPROCMASK:
  case SYSCALL_EXIT_GROUP:
  case SYSCALL_TGKILL:
  case SYSCALL_PRLIMIT64:
    Result = 0;
  break;
  }
  return Result;
}
}
