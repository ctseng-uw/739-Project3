#include <fcntl.h>
#include <unistd.h>

#include <mutex>

#include "HeartbeatClient.hpp"
#include "ServerState.cpp"
#include "includes/blockstore.grpc.pb.h"
#include "includes/blockstore.pb.h"
#include "macro.h"

using grpc::ServerContext;
using grpc::Status;

class BlockStoreServiceImpl final : public hadev::BlockStore::Service {
 public:
  explicit BlockStoreServiceImpl(
      std::shared_ptr<HeartbeatClient> heartbeat_client,
      std::shared_ptr<ServerState> server_state)
      : heartbeat_client(heartbeat_client), server_state(server_state) {
    fd = open("server_device.bin", O_CREAT | O_RDWR, 0644);
    CHK(fd);
  };

 private:
  int fd;
  std::mutex mutex;
  std::shared_ptr<HeartbeatClient> heartbeat_client;
  std::shared_ptr<ServerState> server_state;

  Status Write(ServerContext *context, const hadev::WriteRequest *req,
               hadev::WriteReply *reply) {
    assert(req->data().length() == BLOCK_SIZE);
    mutex.lock();
    CHK(lseek(fd, req->addr(), SEEK_SET));
    assert(write(fd, req->data().c_str(), BLOCK_SIZE) == BLOCK_SIZE);
    CHK(fsync(fd));
    bool ok = false;
    if (server_state->IsBackupAlive() == true) {
      auto status = heartbeat_client->Write(req->addr(), req->data());
      ok = status.ok();
    }
    if (!ok) {
      puts("Write to log");
      server_state->DeclareBackupDead();
      server_state->Add(req->addr(), req->data());
    }
    mutex.unlock();
    reply->set_ret(0);
    return Status::OK;
  }

  Status Read(ServerContext *context, const hadev::ReadRequest *req,
              hadev::ReadReply *reply) {
    lseek(fd, req->addr(), SEEK_SET);
    char buf[BLOCK_SIZE];
    read(fd, buf, BLOCK_SIZE);
    reply->set_data(std::string(buf, BLOCK_SIZE));
    reply->set_ret(0);
    return Status::OK;
  }
};
