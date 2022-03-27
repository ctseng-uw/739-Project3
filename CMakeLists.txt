cmake_minimum_required(VERSION 3.15)
project(Fs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" ${CMAKE_MODULE_PATH})
include(FetchContent)

fetchcontent_declare(
  gRPC
  GIT_REPOSITORY https://github.com/grpc/grpc
  GIT_TAG v1.45.0
)
set(FETCHCONTENT_QUIET OFF)
fetchcontent_makeavailable(gRPC)

add_custom_command(
  OUTPUT includes/hello.pb.cc includes/hello.grpc.pb.cc
  COMMAND ./gen_proto.sh
  DEPENDS protos/hello.proto protoc
)

add_custom_command(
  OUTPUT includes/heartbeat.pb.cc includes/heartbeat.grpc.pb.cc
  COMMAND ./gen_proto.sh
  DEPENDS protos/heartbeat.proto protoc
)

add_executable(server server.cpp heartbeat.cpp includes/heartbeat.pb.cc includes/heartbeat.grpc.pb.cc)
target_link_libraries(server grpc++)
