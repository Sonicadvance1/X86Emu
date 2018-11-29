#include "Core/CPU/CPUCore.h"
#include "LogManager.h"
#include "Syscalls.h"
#include <cstring>
#include <tuple>

namespace Emu {

uint64_t SyscallHandler::HandleSyscall(SyscallArguments *Args) {
  uint64_t Result = 0;
  switch (Args->Argument[0]) {
  default:
    Result = -1;
    LogMan::Msg::D("Unknown syscall\n");
  break;
  // ID Management
  case SYSCALL_GETUID:
    Result = threadmanager.GetUID();
  break;
  case SYSCALL_GETGID:
    Result = threadmanager.GetGID();
  break;
  case SYSCALL_GETEUID:
    Result = threadmanager.GetEUID();
  break;
  case SYSCALL_GETEGID:
    Result = threadmanager.GetEGID();
  break;
  case SYSCALL_GETTID:
    Result = threadmanager.GetTID();
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
    printf("Program exited with: %zd\n", Args->Argument[1]);
    cpu->StopRunning = true;
  break;
  case SYSCALL_ARCH_PRCTL:
    switch (Args->Argument[1]) {
    case 0x1002: // ARCH_SET_FS
      cpu->SetFS(Args->Argument[2]);
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
      cpu->MapRegion(DataOffset, 0x10000000);
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
  // XXX: Currently unhandled bit hit
  case SYSCALL_MMAP:
  case SYSCALL_RT_SIGACTION:
  case SYSCALL_RT_SIGPROCMASK:
  case SYSCALL_FUTEX:
  case SYSCALL_EXIT_GROUP:
  case SYSCALL_TGKILL:
    Result = 0;
  break;
  }
  return Result;
}
}
