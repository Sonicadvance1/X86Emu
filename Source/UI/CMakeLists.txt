set(NAME Test)
set(SRCS TestHarness.cpp)

add_executable(${NAME} ${SRCS})
target_link_libraries(${NAME} Core SonicUtils unicorn pthread LLVM)

set(NAME HostInterface)
set(SRCS HostInterface.cpp)

add_executable(${NAME} ${SRCS})
target_link_libraries(${NAME} Core SonicUtils unicorn pthread LLVM)
