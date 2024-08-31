# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

cmake_minimum_required(VERSION 3.5)
project(LMCAS-Compiler-Pass LANGUAGES CXX)
enable_testing()

# Enable exporting of compile_commands.json, which can help clangd find headers
# in the Nix store that it wouldn't normally be able to find. Just run this once
# and link this wherever development is happening and then further invocations
# to ninja should automatically update it.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")
if(CMAKE_EXPORT_COMPILE_COMMANDS)
  set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES
    ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
endif()

find_package(LLVM REQUIRED CONFIG)
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})

find_package(absl REQUIRED)
find_package(spdlog REQUIRED)

set(CMAKE_CXX_STANDARD 20)
include_directories(include)

add_library(LmcasSpecializationPass MODULE
  src/Annotation.cpp
  src/FindNeck.cpp
  src/InsertMainArgs.cpp
  src/MakeDispatchMain.cpp
  src/MakeNeckContinuation.cpp
  src/Pass.cpp
  src/Spec.cpp
  src/SyscallHandlers.cpp
  src/SyscallVisitor/Structures.cpp
  src/SyscallVisitor/bind.cpp
  src/SyscallVisitor/brk.cpp
  src/SyscallVisitor/connect.cpp
  src/SyscallVisitor/clock_getres.cpp
  src/SyscallVisitor/clock_gettime.cpp
  src/SyscallVisitor/close.cpp
  src/SyscallVisitor/epoll_create1.cpp
  src/SyscallVisitor/fcntl.cpp
  src/SyscallVisitor/fstat.cpp
  src/SyscallVisitor/geteuid.cpp
  src/SyscallVisitor/getgid.cpp
  src/SyscallVisitor/getgroups.cpp
  src/SyscallVisitor/getpid.cpp
  src/SyscallVisitor/getppid.cpp
  src/SyscallVisitor/getuid.cpp
  src/SyscallVisitor/ioctl.cpp
  src/SyscallVisitor/listen.cpp
  src/SyscallVisitor/lseek.cpp
  src/SyscallVisitor/mkdir.cpp
  src/SyscallVisitor/mmap.cpp
  src/SyscallVisitor/mprotect.cpp
  src/SyscallVisitor/munmap.cpp
  src/SyscallVisitor/open.cpp
  src/SyscallVisitor/openat.cpp
  src/SyscallVisitor/pipe.cpp
  src/SyscallVisitor/pread.cpp
  src/SyscallVisitor/prlimit.cpp
  src/SyscallVisitor/read.cpp
  src/SyscallVisitor/readv.cpp
  src/SyscallVisitor/rt_sigaction.cpp
  src/SyscallVisitor/rt_sigprocmask.cpp
  src/SyscallVisitor/sched_getaffinity.cpp
  src/SyscallVisitor/setsockopt.cpp
  src/SyscallVisitor/socket.cpp
  src/SyscallVisitor/stat.cpp
  src/SyscallVisitor/umask.cpp
  src/SyscallVisitor/uname.cpp
  src/SyscallVisitor/write.cpp
  src/SyscallVisitor/writev.cpp
  src/SyscallWrappers.cpp
  src/Tape.cpp
  src/UnrollTape.cpp
  src/UpdateInstructionUtils.cpp
)

target_link_libraries(LmcasSpecializationPass PRIVATE absl::node_hash_map)
target_link_libraries(LmcasSpecializationPass PRIVATE absl::node_hash_set)
target_link_libraries(LmcasSpecializationPass PRIVATE spdlog::spdlog_header_only)
target_compile_features(LmcasSpecializationPass PRIVATE cxx_range_for cxx_auto_type)
set_target_properties(LmcasSpecializationPass PROPERTIES COMPILE_FLAGS "-fno-rtti")

install(TARGETS LmcasSpecializationPass DESTINATION lib)
