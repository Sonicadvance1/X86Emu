#include "Memmap.h"
#include "LogManager.h"
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>

namespace Emu {
bool Memmap::AllocateSHMRegion(size_t Size) {
  const std::string SHMName = "EmuSHM";
  SHMfd = shm_open(SHMName.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600);
  if (SHMfd == -1) {
    LogMan::Msg::E("Couldn't open SHM");
    return false;
  }

  // Unlink the shm file immediately to not leave it around
  shm_unlink(SHMName.c_str());

  // Extend the SHM to the size we requested
  if (ftruncate(SHMfd, Size) != 0) {
    LogMan::Msg::E("Couldn't set SHM size");
    return false;
  }

  SHMSize = Size;
  Base = MapRegion(0, SHMSize, false);
  printf("Base Ptr: %p\n", Base);
  return true;
}

void *Memmap::MapRegion(size_t Offset, size_t Size, bool Fixed) {
  void *Ptr = mmap((void*)((uintptr_t)Base+Offset), Size, PROT_READ | PROT_WRITE,
      MAP_SHARED | (Fixed ? MAP_FIXED : 0), SHMfd, Offset);

  if (Ptr == MAP_FAILED) {
    LogMan::Msg::A("Failed to map memory region");
    return nullptr;
  }
  printf("Mapped region: 0x%zx -> %p %zx\n", Offset, Ptr, Size);
  MappedRegions.emplace_back(MemRegion{Ptr, Offset, Size});
  return Ptr;
}

void Memmap::UnmapRegion(void *Ptr, size_t Size) {
  auto it = std::find(MappedRegions.begin(), MappedRegions.end(), Ptr);
  if (it != MappedRegions.end()) {
    munmap(Ptr, Size);
    MappedRegions.erase(it);
  }
}

void Memmap::DeallocateSHMRegion() {
  close(SHMfd);
}


void *Memmap::GetPointer(uint64_t Offset) {
  for (const auto &Region : MappedRegions) {
    if (Offset >= Region.Offset &&
        Offset < (Region.Offset + Region.Size))
      return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(Region.Ptr) + (Offset - Region.Offset));
  }
  return nullptr;
}

}
