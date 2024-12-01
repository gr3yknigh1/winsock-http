cmake_minimum_required(VERSION 3.20)
project(winsock-http LANGUAGES C CXX)

if(MSVC)
  add_compile_options(
    /W4
    /Wall
  )
endif()

add_executable(http_server
  WIN32
  ${CMAKE_CURRENT_SOURCE_DIR}/http_server.cpp
)

target_compile_features(http_server
  PRIVATE cxx_std_14
)

target_link_options(http_server PRIVATE "/subsystem:console")

add_executable(http_client
  ${CMAKE_CURRENT_SOURCE_DIR}/http_client.cpp
)

target_compile_features(http_client
  PRIVATE cxx_std_14
)
