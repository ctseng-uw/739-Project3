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
#include <thread>

#include "BlockStoreServiceImpl.cpp"
#include "HeartbeatClient.hpp"
#include "HeartbeatServiceImpl.hpp"
#include "ServerState.cpp"
#include "grpcpp/resource_quota.h"

using grpc::ServerBuilder;

const std::array<std::string, 2> LAN_ADDR{"server0", "server1"};
const std::string PORT = "50051";
const uint32_t TIMEOUTMS = 10;

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr << "usage: " << argv[0]
              << " <node_number, 0 or 1> <i_am_primary, 0 or 1>\n";
    exit(1);
  }

  int my_node_number = atoi(argv[1]);
  bool i_am_primary = atoi(argv[2]);
  auto i_am_primary_ptr = std::make_shared<bool>(i_am_primary);

  auto server_state = std::make_shared<ServerState>(i_am_primary);

  grpc::ChannelArguments ch_args;
  ch_args.SetMaxReceiveMessageSize(INT_MAX);
  ch_args.SetMaxSendMessageSize(INT_MAX);

  auto heartbeat_client = std::make_shared<HeartbeatClient>(
      grpc::CreateCustomChannel(LAN_ADDR[1 - my_node_number] + ":" + PORT,
                                grpc::InsecureChannelCredentials(), ch_args));

  BlockStoreServiceImpl blockstore_service(heartbeat_client, server_state);
  HeartbeatServiceImpl heartbeat_service(i_am_primary_ptr);

  grpc::ServerBuilder builder;
  const std::string server_address = "0.0.0.0:" + PORT;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.SetMaxSendMessageSize(INT_MAX);
  builder.SetMaxReceiveMessageSize(INT_MAX);
  builder.SetMaxMessageSize(INT_MAX);
  builder.RegisterService(&blockstore_service);
  builder.RegisterService(&heartbeat_service);
  std::cout << "Server listening on " << server_address << std::endl;
  auto server = builder.BuildAndStart();

  while (true) {
    server_state->WaitUntilIsPrimary();
    try {
      heartbeat_client->BeatHeart();
      if (server_state->IsBackupAlive() == false) {
        puts("Starting recovery");
        while (true) {
          auto [addr, data] = server_state->Pop();
          if (addr == -1) break;
          heartbeat_client->Write(addr, *data);
        }
        server_state->DeclareBackupNormal();
      }
      usleep(TIMEOUTMS * 300);
    } catch (const std::exception &e) {
      server_state->DeclareBackupDead();
      // std::cerr << e.what() << '\n';
    }
  }

  return 0;
}
