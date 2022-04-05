#include <fcntl.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/server_context.h>
#include <grpcpp/support/status.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>
#include <mutex>
#include <thread>

#include "SingleBlockStoreServiceImpl.cpp"
#include "grpcpp/resource_quota.h"
#include "magic.h"
#include "macro.h"

using grpc::ServerBuilder;


int main(int argc, char **argv) {
  SingleBlockStoreServiceImpl blockstore_service;

  grpc::ServerBuilder builder;
  const std::string server_address = "0.0.0.0:" + PORT;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.SetMaxSendMessageSize(INT_MAX);
  builder.SetMaxReceiveMessageSize(INT_MAX);
  builder.SetMaxMessageSize(INT_MAX);
  builder.RegisterService(&blockstore_service);
  std::cout << "Server listening on " << server_address << std::endl;
  std::cout << MAGIC_SERVER_START << std::endl;
  auto server = builder.BuildAndStart();

  while (true) {;}

  return 0;
}
