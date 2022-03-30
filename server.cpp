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

#include "BlockStoreServiceImpl.cpp"
#include "HeartbeatClient.cpp"
#include "HeartbeatServiceImpl.hpp"
#include "grpcpp/resource_quota.h"
#include "macro.h"

using grpc::ServerBuilder;

const std::array<std::string, 2> LAN_ADDR{"server0", "server1"};

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: " << argv[0]
              << " <node_number, 0 or 1>\n";
    exit(1);
  }

  int my_node_number = atoi(argv[1]);
  int i_am_primary = 0; // 0: init, 1: backup, 2: primary
  auto i_am_primary_ptr = std::make_shared<int>(i_am_primary);
  struct timespec last_heartbeat;
  auto last_heartbeat_ptr = std::make_shared<struct timespec>(last_heartbeat);

  grpc::ChannelArguments ch_args;
  ch_args.SetMaxReceiveMessageSize(INT_MAX);
  ch_args.SetMaxSendMessageSize(INT_MAX);

  auto heartbeat_client = std::make_shared<HeartbeatClient>(
      grpc::CreateCustomChannel(LAN_ADDR[1 - my_node_number] + ":" +
                                std::to_string(PORT),
                                grpc::InsecureChannelCredentials(), ch_args),
      i_am_primary_ptr, last_heartbeat_ptr, my_node_number);

  BlockStoreServiceImpl blockstore_service(heartbeat_client);
  HeartbeatServiceImpl heartbeat_service(i_am_primary_ptr, last_heartbeat_ptr);

  grpc::ServerBuilder builder;
  const std::string server_address = "0.0.0.0:" + std::to_string(PORT);
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.SetMaxSendMessageSize(INT_MAX);
  builder.SetMaxReceiveMessageSize(INT_MAX);
  builder.SetMaxMessageSize(INT_MAX);
  builder.RegisterService(&blockstore_service);
  builder.RegisterService(&heartbeat_service);
  std::cout << "Server listening on " << server_address << std::endl;
  auto server = builder.BuildAndStart();

  heartbeat_client->Start();

  return 0;
}
