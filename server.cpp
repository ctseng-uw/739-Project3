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

#include "blockstore.cpp"
#include "heartbeat.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

const std::array<std::string, 2> LAN_ADDR{"127.0.0.1", "127.0.0.1"};
const std::string HEARTTBEATPORT = "53705";
const std::string BLOCKSTOREPORT = "50051";
const uint32_t TIMEOUTMS = 10;

template <typename T>
std::unique_ptr<grpc::Server> create_server(const std::string &server_address,
                                            T *service) {
  grpc::ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.SetMaxSendMessageSize(INT_MAX);
  builder.SetMaxReceiveMessageSize(INT_MAX);
  builder.SetMaxMessageSize(INT_MAX);
  builder.RegisterService(service);
  std::cout << "Server listening on " << server_address << std::endl;
  return builder.BuildAndStart();
}

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr << "usage: " << argv[0]
              << " <node_number, 0 or 1> <i_am_primary, 0 or 1>\n";
    exit(1);
  }
  int my_node_number = atoi(argv[1]);
  auto i_am_primary = std::make_shared<bool>(atoi(argv[2]));

  grpc::ChannelArguments ch_args;
  ch_args.SetMaxReceiveMessageSize(INT_MAX);
  ch_args.SetMaxSendMessageSize(INT_MAX);

  auto heartbeat_client =
      std::make_shared<HeartbeatClient>(grpc::CreateCustomChannel(
          LAN_ADDR[1 - my_node_number] + ":" + HEARTTBEATPORT,
          grpc::InsecureChannelCredentials(), ch_args));

  BlockStoreServiceImpl blockstore_service(heartbeat_client);
  auto blockstore_server =
      create_server("0.0.0.0:" + BLOCKSTOREPORT, &blockstore_service);

  HeartbeatServiceImpl heartbeat_service(i_am_primary);
  auto heartbeat_server =
      create_server("0.0.0.0:" + HEARTTBEATPORT, &heartbeat_service);

  while (true) {
    // TODO: conditional var?
    while (i_am_primary) {
      try {
        heartbeat_client->BeatHeart();
        std::cout << "beat" << ' ' << std::flush;
        usleep(TIMEOUTMS * 300);
      } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
      }
    }
  }

  return 0;
}
