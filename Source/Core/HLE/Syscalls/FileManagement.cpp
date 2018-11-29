#include <sys/stat.h>
#include <unistd.h>
#include "Core/CPU/CPUCore.h"
#include "FileManagement.h"

namespace Emu {
ssize_t FD::writev(int fd, void *iov, int iovcnt) {
  return -1;
  ssize_t FinalSize = 0;
  printf("writev: %d 0x%p %d\n", fd, iov, iovcnt);
  for (int i = 0; i < iovcnt; ++i) {
    struct iovStruct {
      uint64_t iov_base;
      size_t iov_len;
    } *iovObject;

    iovObject = (iovStruct*)iov;
    const char *DataString = cpu->MemoryMapper->GetPointer<const char*>(iovObject->iov_base);
    printf("\t 0x%zx Size: 0x%zx\n", iovObject->iov_base, iovObject->iov_len);
    printf("\t %s\n", DataString);
    FinalSize += iovObject->iov_len;

  }
  return FinalSize;
}

uint64_t FileManager::Read(int fd, void *buf, size_t count) {
  printf("XXX: Implement Read\n");
  return 0;
}

uint64_t FileManager::Write(int fd, void *buf, size_t count) {
  return write(fd, buf, count);
}

uint64_t FileManager::Open(const char* pathname, int flags, uint32_t mode) {
  printf("XXX: Implement Open\n");
  return 0;
}

uint64_t FileManager::Close(int fd) {
  printf("XXX: Implement Close\n");
  return 0;
}

uint64_t FileManager::Fstat(int fd, void *buf) {
  if (fd == STDOUT_FILENO || fd == STDERR_FILENO) {
    return fstat(fd, (struct stat*)buf);
  }
  else {
    printf("Attempting to stat: %d\n", fd);
    return -1LL;
  }
}

uint64_t FileManager::Lseek(int fd, uint64_t offset, int whence) {
  printf("XXX: Implement Lseek\n");
  return 0;
}

uint64_t FileManager::Writev(int fd, void *iov, int iovcnt) {
    auto fdp = FDMap.find(fd);
    if (fdp == FDMap.end()) {
      printf("XXX: Trying to open unknown fd: %d\n", fd);
      return -1LL;
    }
    return fdp->second->writev(fd, iov, iovcnt);
}

uint64_t FileManager::Access(const char *pathname, int mode) {
  printf("Trying to read access of : %s\n", pathname);
  return -1LL;
}

uint64_t FileManager::Readlink(const char *path, char *buf, size_t bufsiz) {
  printf("Trying to readlink of : %s\n", path);
  return -1LL;
}

uint64_t FileManager::Openat(int dirfd, const char *pathname, int flags, uint32_t mode) {
    int32_t fdNum = CurrentFDOffset;
    printf("Opened file: %s with fd: %d\n", pathname, fdNum);
    FD FDObject{cpu, fdNum, pathname, flags, mode};
    auto fd = &FDs.emplace_back(FDObject);
    FDMap[CurrentFDOffset] = fd;
    CurrentFDOffset++;
    return fdNum;
}
}
