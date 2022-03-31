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
#include "magic.h"

using grpc::ServerBuilder;

const std::array<std::string, 2> LAN_ADDR{"node0", "node1"};

/*
 * Server always start at ServerState::INIT.
 * GetState() is only called when server starts, to know other's state. Until
 * this information is known, no client request will be served.
 * Both HeartbeatClient::GetState()'s return && HeartbeatService::GetState()
 * (request handling code) can change ServerState.
 * - ServerState::INIT:
 *   - (BlockStoreService) Return "I am INIT" message.
 *   - (HeartbeatClient) keep issuing GetState() to ask the other node's state
 *     until getting the answer.
 *     - Recheck state is still INIT. Otherwise no-op.
 *     - If the other node is BACKUP: This occurs when both start as INIT and my
 * node number is smaller than the other. Turn myself to PRIMARY.
 *     - If the other node is PRIMARY: It means I was a recovered BACKUP. Turn
 * my state to BACKUP.
 *   - (HeartbeatService)
 *     - on receiving GetState():
 *       - Both nodes are in INIT. If node_num == 0, change state to
 * PRIMARY; if node_num == 1, change state to BACKUP. Return changed state.
 *     - on receiving RepliWrite():
 *       - Change state to BACKUP. Assert this (addr, data) is empty (i.e. this
 * is a heartbeat, not a write request). Ack.
 *
 * - ServerState::BACKUP:
 *   - (BlockStoreService) return "I am Backup" message.
 *   - (HeartbeatService) receive GetState():
 *     - previous PRIMARY was dead. Change itself to PRIMARY and return
 *   - (HeartbeatClient) should never be used.
 *
 * - ServerState::PRIMARY
 *   - (BlockStoreService) Do the logic.
 *   - (HeartbeatService) receive GetState(): return PRIMARY
 *   - (HeartbeatClient) Do the logic.
 */

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: " << argv[0] << " <node_number, 0 or 1>\n";
    exit(1);
  }

  int node_num = atoi(argv[1]);
  std::shared_ptr<ServerState> server_state =
      std::make_shared<ServerState>(ServerState::INIT);
  std::shared_ptr<timespec> last_heartbeat =
      std::make_shared<struct timespec>();

  grpc::ChannelArguments ch_args;
  ch_args.SetMaxReceiveMessageSize(INT_MAX);
  ch_args.SetMaxSendMessageSize(INT_MAX);

  auto heartbeat_client = std::make_shared<HeartbeatClient>(
      grpc::CreateCustomChannel(
          LAN_ADDR[1 - node_num] + ":" + std::to_string(PORT),
          grpc::InsecureChannelCredentials(), ch_args),
      server_state, last_heartbeat, node_num);

  BlockStoreServiceImpl blockstore_service(server_state, heartbeat_client);
  HeartbeatServiceImpl heartbeat_service(server_state, last_heartbeat);

  grpc::ServerBuilder builder;
  const std::string server_address = "0.0.0.0:" + std::to_string(PORT);
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.SetMaxSendMessageSize(INT_MAX);
  builder.SetMaxReceiveMessageSize(INT_MAX);
  builder.SetMaxMessageSize(INT_MAX);
  builder.RegisterService(&blockstore_service);
  builder.RegisterService(&heartbeat_service);
  std::cout << "Server listening on " << server_address << std::endl;
  std::cout << MAGIC_SERVER_START << std::endl;
  auto server = builder.BuildAndStart();

  heartbeat_client->Start();

  return 0;
}
