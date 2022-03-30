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


file(GLOB PROTOBUF_SRC "protos/*")
set(PROTOBUF_DST "")
set(PROTOBUF_EXT ".grpc.pb.cc" ".grpc.pb.h" ".pb.cc" ".pb.h")
foreach(file ${PROTOBUF_SRC})
  foreach(ext ${PROTOBUF_EXT})
    get_filename_component(fileh ${file} NAME_WE)
    string(PREPEND fileh "${CMAKE_CURRENT_SOURCE_DIR}/includes/")
    string(APPEND fileh ${ext})
    list(APPEND PROTOBUF_DST ${fileh})
  endforeach()
endforeach()


add_custom_command(
  OUTPUT ${PROTOBUF_DST}
  COMMAND _deps/grpc-build/third_party/protobuf/protoc
  --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/protos
  --cpp_out=${CMAKE_CURRENT_SOURCE_DIR}/includes
  --grpc_out=${CMAKE_CURRENT_SOURCE_DIR}/includes
  --plugin=protoc-gen-grpc=_deps/grpc-build/grpc_cpp_plugin
  ${CMAKE_CURRENT_SOURCE_DIR}/protos/*.proto
  DEPENDS ${PROTOBUF_SRC} protoc
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/includes)
add_executable(server server.cpp HeartbeatServiceImpl.cpp HeartbeatServiceImpl.hpp HeartbeatClient.cpp ${PROTOBUF_DST})
target_link_libraries(server grpc++)

add_library(hadev STATIC BlockStoreClient.cpp ${PROTOBUF_DST})
target_link_libraries(hadev grpc++)

add_executable(client client.cpp)
target_link_libraries(client hadev grpc++)


find_package(FUSE REQUIRED)
include_directories(${FUSE_INCLUDE_DIR})
add_definitions("-D_FILE_OFFSET_BITS=64")
add_executable(mfsfuse fs/io.cpp fs/io.h fs/mfs.cpp fs/mfs.h fs/mfsfuse.cpp)
target_link_libraries(mfsfuse hadev grpc++ ${FUSE_LIBRARIES})
