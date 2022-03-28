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
  OUTPUT protoh/hello.pb.cc protoh/hello.grpc.pb.cc protoh/heartbeat.pb.cc protoh/heartbeat.grpc.pb.cc
  COMMAND ./gen_proto.sh
  DEPENDS protos/hello.proto protos/heartbeat.proto protoc
)


add_executable(server server.cpp heartbeat.cpp heartbeat.h protoh/hello.pb.cc protoh/hello.grpc.pb.cc protoh/heartbeat.pb.cc protoh/heartbeat.grpc.pb.cc)
target_link_libraries(server grpc++)

add_library(hadev STATIC hadev.cpp hadev.hpp protoh/hello.pb.cc protoh/hello.grpc.pb.cc)
target_link_libraries(hadev grpc++)

add_executable(client client.cpp hadev.hpp)
target_link_libraries(client hadev grpc++)


find_package(FUSE REQUIRED)
include_directories(${FUSE_INCLUDE_DIR})
add_definitions("-D_FILE_OFFSET_BITS=64")
add_executable(mfsfuse fs/io.cpp fs/io.h fs/mfs.cpp fs/mfs.h fs/mfsfuse.cpp)
target_link_libraries(mfsfuse hadev grpc++ ${FUSE_LIBRARIES})
