#!/bin/sh
CURR=$(dirname "$0")
echo $CURR
PROTOC_BIN=${CURR}/_deps/grpc-build/third_party/protobuf/protoc
PROTO_DIR=${CURR}/../protos
OUT_CPP=protoh/${CURR}/
OUT_GRPC=protoh/${CURR}/
GRPC_PROTOC_PLUGIN=${CURR}/_deps/grpc-build/grpc_cpp_plugin

mkdir -p protoh
$PROTOC_BIN --proto_path=${PROTO_DIR} --cpp_out=${OUT_CPP} --grpc_out=${OUT_GRPC} --plugin=protoc-gen-grpc=${GRPC_PROTOC_PLUGIN} ../protos/*.proto
