#pragma once
#include <cstdint>
#include <cstddef>
#include <unordered_map>
#include <vector>

namespace Emu {
class CPUCore;
class FD {
public:
  FD(CPUCore *CPU, int fd, const char *pathname, int flags, mode_t mode)
    : cpu{CPU}
    , FDesc{fd}
    , Name{pathname}
    , Flags{flags}
    , Mode{mode} {
  }

  ssize_t writev(int fd, void *iov, int iovcnt);
  CPUCore *cpu;
  int FDesc{0};
  std::string Name;
  int Flags;
  mode_t Mode;
};

class FileManager {
public:
  FileManager(CPUCore *CPU) : cpu {CPU} {}
  uint64_t Read(int fd, void *buf, size_t count);
  uint64_t Write(int fd, void *buf, size_t count);
  uint64_t Open(const char* pathname, int flags, uint32_t mode);
  uint64_t Close(int fd);
  uint64_t Fstat(int fd, void *buf);
  uint64_t Lseek(int fd, uint64_t offset, int whence);
  uint64_t Writev(int fd, void *iov, int iovcnt);
  uint64_t Access(const char *pathname, int mode);
  uint64_t Readlink(const char *path, char *buf, size_t bufsiz);
  uint64_t Openat(int dirfd, const char *pathname, int flags, uint32_t mode);

private:
  CPUCore *cpu;

  int32_t CurrentFDOffset{3};
  std::vector<FD> FDs;
  std::unordered_map<int32_t, FD*> FDMap;
};
}
