# SPDX-FileCopyrightText: 2022-2024 Smart Information Flow Technologies
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

file(GLOB_RECURSE CONFIG_SRC *.h *.cpp)

if(BUILD_SHARED_LIBS)
  add_library(guiness
    SHARED
    ${CONFIG_SRC}
  )
else()
  add_library(guiness
    STATIC
    ${CONFIG_SRC}
  )
endif()

target_link_libraries(guiness PRIVATE spdlog::spdlog_header_only)
target_link_libraries(guiness PRIVATE absl::node_hash_map)
target_link_libraries(guiness PRIVATE absl::node_hash_set)
target_link_libraries(guiness PRIVATE ${llvm_libs})

set_target_properties(guiness
  PROPERTIES
  LINKER_LANGUAGE CXX
  PREFIX "lib"
)

install(TARGETS guiness
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
