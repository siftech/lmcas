add_executable(neck
  neck.cpp
)

target_link_libraries(neck
  LINK_PUBLIC
  neckid_config
  neckid_neckid
  ${Boost_LIBRARIES}
)

phasar_config(neck)

install(TARGETS neck)
