cmake_minimum_required (VERSION 3.6)

project(Automated-Neck-Identification)
set(CMAKE_PROJECT_NAME "Automated-Neck-Identification")

set(CMAKE_EXPORT_COMPILE_COMMANDS YES)
if(CMAKE_EXPORT_COMPILE_COMMANDS)
  set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES
    ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set_property(GLOBAL PROPERTY TARGET_MESSAGES OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "DebugSan" CACHE STRING "Build mode ('DebugSan', 'Debug' or 'Release', default is 'DebugSan')" FORCE)
endif ()

if(CMAKE_BUILD_TYPE STREQUAL "DebugSan")
# Debug build with address and undefined behavior sanitizers enabled
  message(STATUS "Select Debug Build with Sanitizers")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -MP -fstack-protector-strong -g -fno-omit-frame-pointer -fsanitize=address,undefined")
  set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address,undefined")
  # Required to communicate enabled sanitizers to PhASAR
  set(LLVM_USE_SANITIZER "Address,Undefined")
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
# Standard debug build
  message(STATUS "Select Debug Build")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -MP -fstack-protector-strong -g")
else()
# Release build
  message(STATUS "Select Release Build")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -MP -fstack-protector-strong -march=native")
endif()

# Enable testing
enable_testing()

set(NECKID_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${NECKID_SRC_DIR}/cmake")

include("neckid_macros")

option(NECKID_RUN_CLANGTIDY_IN_BUILD "Run clang-tidy during the build (may be expensive on small machines, default is OFF)" OFF)

option(NECKID_BUILD_IR "Build IR test code (default is ON)" ON)

option(BUILD_SHARED_LIBS "Build shared libraries (default is ON)" ON)

option(NECKID_ENABLE_WARNINGS "Enable warnings" ON)
if (NECKID_ENABLE_WARNINGS)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-variable -Wno-unused-parameter -Wno-return-type-c-linkage ")
endif (NECKID_ENABLE_WARNINGS)

option(NECKID_ENABLE_PIC "Build Position-Independed Code" ON)
if (NECKID_ENABLE_PIC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
endif (NECKID_ENABLE_PIC)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

include_directories(
  ${NECKID_SRC_DIR}/include/
)

# Handle RPATH, RUNPATH, etc. THis works when we build it, I have no idea if
# it works after a make install
set(CMAKE_INSTALL_RPATH
	"${CMAKE_INSTALL_PREFIX}/lib")
set(CMAKE_BUILD_RPATH
	 "${CMAKE_CURRENT_BINARY_DIR}/lib")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

if (LLVM_ENABLE_LIBCXX)
  set(NECKID_STD_FILESYSTEM c++fs)
else()
  set(NECKID_STD_FILESYSTEM stdc++fs)
endif()

### Adding external libraries

# absl
find_package(absl REQUIRED)

# spdlog
find_package(spdlog REQUIRED)

# Threads
find_package(Threads)

# Boost
find_package(Boost 1.65.1 COMPONENTS filesystem graph system program_options log ${BOOST_THREAD} REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

# Disable clang-tidy for the external projects
set(CMAKE_CXX_CLANG_TIDY "")

# PhASAR
find_package(phasar
  COMPONENTS
  config
  controlflow
  db
  ifdside
  phasarllvm_utils
  pointer
  taintconfig
  typehierarchy
  utils
  REQUIRED)
add_custom_target(LLFileGeneration ALL)
include_directories(${PHASAR_INCLUDE_DIR})
link_directories(${PHASAR_LIBRARY_DIR})

# LLVM
find_package(LLVM 12 REQUIRED CONFIG)
set(USE_LLVM_FAT_LIB ON)

# the remaining LLVM related setup is performed by PhASAR

# Set up clang-tidy to be applied during compilation to indicate code smells
if (NECKID_RUN_CLANGTIDY_IN_BUILD)
  set(CMAKE_CXX_CLANG_TIDY
      clang-tidy;
      -header-filter=NeckID.*;
    # -warnings-as-errors=*;
  )
endif()

# Add the Neck-ID subdirectories
add_subdirectory(config)
add_subdirectory(include)
add_subdirectory(lib)
add_subdirectory(tools)

if (NECKID_BUILD_IR)
  message("Building IR Test Code")
  add_subdirectory(test)
endif()
