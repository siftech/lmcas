add_executable(constantconv
  constantconv.cpp
)

target_link_libraries(constantconv
  LINK_PUBLIC
  ${Boost_LIBRARIES}
)

phasar_config(constantconv)
