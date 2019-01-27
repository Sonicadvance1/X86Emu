#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <tuple>
#include "FileManagement.h"
#include "ThreadManagement.h"

namespace Emu {
class CPUCore;

struct Futex {
  std::mutex mute;
  std::condition_variable cv;
  std::atomic<uint32_t> *addr;
  uint32_t val;
};
// Enum containing all support x86-64 linux syscalls
enum Syscalls {
  SYSCALL_READ            = 0,   ///< __NR_read
  SYSCALL_WRITE           = 1,   ///< __NR_write
  SYSCALL_OPEN            = 2,   ///< __NR_open
  SYSCALL_CLOSE           = 3,   ///< __NR_close
  SYSCALL_FSTAT           = 5,   ///< __NR_fstat
  SYSCALL_LSEEK           = 8,   ///< __NR_lseek
  SYSCALL_MMAP            = 9,   ///< __NR_mmap
  SYSCALL_MPROTECT        = 10,  ///< __NR_mprotect
  SYSCALL_BRK             = 12,  ///< __NR_brk
  SYSCALL_RT_SIGACTION    = 13,  ///< __NR_rt_sigaction
  SYSCALL_RT_SIGPROCMASK  = 14,  ///< __NR_rt_sigprocmask
  SYSCALL_WRITEV          = 20,  ///< __NR_writev
  SYSCALL_ACCESS          = 21,  ///< __NR_access
  SYSCALL_NANOSLEEP       = 35,  ///< __NR_nanosleep
  SYSCALL_GETPID          = 39,  ///< __NR_getpid
  SYSCALL_CLONE           = 56,  ///< __NR_clone
  SYSCALL_EXIT            = 60,  ///< __NR_exit
  SYSCALL_UNAME           = 63,  ///< __NR_uname
  SYSCALL_READLINK        = 89,  ///< __NR_readlink
  SYSCALL_GETUID          = 102, ///< __NR_getuid
  SYSCALL_GETGID          = 104, ///< __NR_getgid
  SYSCALL_GETEUID         = 107, ///< __NR_geteuid
  SYSCALL_GETEGID         = 108, ///< __NR_getegid
  SYSCALL_ARCH_PRCTL      = 158, ///< __NR_arch_prctl
  SYSCALL_GETTID          = 186, ///< __NR_gettid
  SYSCALL_FUTEX           = 202, ///< __NR_futex
  SYSCALL_SET_TID_ADDRESS = 218, ///< __NR_set_tid_address
  SYSCALL_CLOCK_GETTIME   = 228, ///< __NR_clock_gettime
  SYSCALL_EXIT_GROUP      = 231, ///< __NR_exit_group
  SYSCALL_TGKILL          = 234, ///< __NR_tgkill
  SYSCALL_OPENAT          = 257, ///< __NR_openat
  SYSCALL_SET_ROBUST_LIST = 273, ///< __NR_set_robust_list
  SYSCALL_PRLIMIT64       = 302, ///< __NR_prlimit64
};

class SyscallHandler final {
public:
  struct SyscallArguments {
    static constexpr std::size_t MAX_ARGS = 7;
    uint64_t Argument[MAX_ARGS];
  };
  SyscallHandler(CPUCore *CPU) : cpu {CPU}, filemanager {CPU} {}
  uint64_t HandleSyscall(SyscallArguments *Args);

  void EmplaceFutex(uint64_t Addr, Futex *futex) {
    std::scoped_lock<std::mutex> lk(FutexMutex);
    Futexes[Addr] = futex;
  }
  Futex *GetFutex(uint64_t Addr) {
    std::scoped_lock<std::mutex> lk(FutexMutex);
    return Futexes[Addr];
  }
private:
  CPUCore *cpu;
  FileManager filemanager;
  std::map<uint64_t, Futex*> Futexes;
  std::mutex FutexMutex;

  // BRK management
  uint64_t dataspace {};
  uint64_t dataspacesize{};
};
}
