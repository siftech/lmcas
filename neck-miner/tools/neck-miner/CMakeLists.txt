add_executable(neck-miner
  neck-miner.cpp
)

target_link_libraries(neck-miner
  PUBLIC
  neckid_config
  neckid_neckid
  guiness
  ${Boost_LIBRARIES}
)

phasar_config(neck-miner)

target_link_libraries(neck-miner PRIVATE absl::node_hash_map)
target_link_libraries(neck-miner PRIVATE absl::node_hash_set)
target_link_libraries(neck-miner PRIVATE spdlog::spdlog_header_only)
target_link_libraries(neck-miner PRIVATE ${llvm_libs})

install(TARGETS neck-miner)
