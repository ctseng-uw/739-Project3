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
#include "TimeoutWatcher.cpp"
#include "grpcpp/resource_quota.h"
#include "magic.h"

using grpc::ServerBuilder;

const std::array<std::string, 2> LAN_ADDR{"node0", "node1"};
const std::string PORT = "23576";

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr << "usage: " << argv[0]
              << " <node_number, 0 or 1> <i_am_primary, 0 or 1>\n";
    exit(1);
  }

  int my_node_number = atoi(argv[1]);
  // TODO: ugly
  auto i_am_primary = std::make_shared<bool>(atoi(argv[2]));

  auto watcher = std::make_shared<TimeoutWatcher>();

  grpc::ChannelArguments ch_args;
  ch_args.SetMaxReceiveMessageSize(INT_MAX);
  ch_args.SetMaxSendMessageSize(INT_MAX);
  ch_args.SetInt(GRPC_ARG_MAX_RECONNECT_BACKOFF_MS, 100);

  auto heartbeat_client = std::make_shared<HeartbeatClient>(
      grpc::CreateCustomChannel(LAN_ADDR[1 - my_node_number] + ":" + PORT,
                                grpc::InsecureChannelCredentials(), ch_args),
      i_am_primary);

  BlockStoreServiceImpl blockstore_service(i_am_primary, heartbeat_client);
  HeartbeatServiceImpl heartbeat_service(i_am_primary, watcher, my_node_number);

  grpc::ServerBuilder builder;
  const std::string server_address = "0.0.0.0:" + PORT;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.SetMaxSendMessageSize(INT_MAX);
  builder.SetMaxReceiveMessageSize(INT_MAX);
  builder.SetMaxMessageSize(INT_MAX);
  builder.RegisterService(&blockstore_service);
  builder.RegisterService(&heartbeat_service);
  std::cout << "Server listening on " << server_address << std::endl;
  std::cout << MAGIC_SERVER_START << std::endl;
  auto server = builder.BuildAndStart();

  if (*i_am_primary)
    puts("Start as PRIMARY");
  else
    puts("Start as BACKUP");
  while (true) {
    if (*i_am_primary) {
      heartbeat_client->BlockUntilBecomeBackup();  // Keep sending heartbeat
      puts("Exit BlockUntilBecomeBackup. Turned to BACKUP");
    } else {
      watcher->BlockUntilHeartbeatTimeout();
      puts("Exit BlockUntilHeartbeatTimeout.");
      puts("BACKUP to PRIMARY");
      *i_am_primary = true;
      std::cout << MAGIC_BECOMES_PRIMARY << std::endl;
    }
  }

  return 0;
}
