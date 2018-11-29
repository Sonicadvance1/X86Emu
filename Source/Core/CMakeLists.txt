set(NAME Core)
set(SRCS
  Bootloader/Bootloader.cpp
  Bootloader/ELFLoader.cpp
  CPU/BlockCache.cpp
  CPU/CPUCore.cpp
  CPU/IR.cpp
  CPU/OpcodeDispatch.cpp
  CPU/PassManager.cpp
  CPU/X86Tables.cpp
  CPU/AArch64Backend/AArch64.cpp
  CPU/InterpreterBackend/Interpreter.cpp
  CPU/LLVMBackend/LLVM.cpp
  HLE/Syscalls/Syscalls.cpp
  HLE/Syscalls/FileManagement.cpp
  Core.cpp
  Memmap.cpp)

add_library(${NAME} STATIC ${SRCS} )
target_link_libraries(${NAME} rt)
