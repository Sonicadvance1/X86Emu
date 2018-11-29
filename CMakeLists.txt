cmake_minimum_required(VERSION 3.10)
project(Emu)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Bin)

add_definitions(-Wno-trigraphs)

add_subdirectory(External/SonicUtils/)

include_directories(External/SonicUtils/)
include_directories(Source/)

find_package(LLVM CONFIG QUIET)
if(LLVM_FOUND AND TARGET LLVM)
  message(STATUS "LLVM found!")
  include_directories(${LLVM_INCLUDE_DIRS})
endif()


add_subdirectory(Source/)
