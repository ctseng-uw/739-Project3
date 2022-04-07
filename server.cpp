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
#include "macro.h"
#include "magic.h"

using grpc::ServerBuilder;

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr << "usage: " << argv[0]
              << " <node_number, 0 or 1> <i_am_primary, 0 or 1>\n";
    exit(1);
  }

  int my_node_number = atoi(argv[1]);
  auto i_am_primary = std::make_shared<bool>(atoi(argv[2]));

  auto watcher = std::make_shared<TimeoutWatcher>();

    auto heartbeat_client =
      std::make_shared<HeartbeatClient>(my_node_number, i_am_primary);

  BlockStoreServiceImpl blockstore_service(i_am_primary, heartbeat_client);
  HeartbeatServiceImpl heartbeat_service(i_am_primary, watcher, my_node_number,
                                         heartbeat_client);

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
    std::cout << "Start as PRIMARY" << std::endl;
  else
    std::cout << "Start as BACKUP" << std::endl;
  while (true) {
    if (*i_am_primary) {
      heartbeat_client->BlockUntilBecomeBackup();  // Keep sending heartbeat
      std::cout << "Exit BlockUntilBecomeBackup. Turned to BACKUP" << std::endl;
    } else {
      watcher->BlockUntilHeartbeatTimeout();
      std::cout << "Exit BlockUntilHeartbeatTimeout." << std::endl;
      std::cout << "BACKUP to PRIMARY" << std::endl;
      *i_am_primary = true;
      std::cout << MAGIC_BECOMES_PRIMARY << std::endl;
    }
  }

  return 0;
}
