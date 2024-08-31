# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

cmake_minimum_required(VERSION 3.5)
project(LmcasAnnotationPass LANGUAGES CXX)
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

find_package(spdlog REQUIRED)

set (CMAKE_CXX_STANDARD 17)
include_directories(include)

add_library(LmcasAnnotationPass MODULE
  src/Pass.cpp
)

target_link_libraries(LmcasAnnotationPass PRIVATE spdlog::spdlog_header_only)
target_compile_features(LmcasAnnotationPass PRIVATE cxx_range_for cxx_auto_type)
set_target_properties(LmcasAnnotationPass PROPERTIES COMPILE_FLAGS "-fno-rtti")

install(TARGETS LmcasAnnotationPass DESTINATION lib)
