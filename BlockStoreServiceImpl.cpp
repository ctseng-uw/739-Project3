#include <fcntl.h>
#include <unistd.h>

#include <mutex>

#include "HeartbeatClient.hpp"
#include "includes/blockstore.grpc.pb.h"
#include "includes/blockstore.pb.h"
#include "macro.h"

using grpc::ServerContext;
using grpc::Status;

constexpr unsigned long BLOCK_SIZE = 4096;

class BlockStoreServiceImpl final : public hadev::BlockStore::Service {
 public:
  BlockStoreServiceImpl(std::shared_ptr<HeartbeatClient> heartbeat_client)
      : heartbeat_client(heartbeat_client) {
    fd = open("server_device.bin", O_CREAT | O_RDWR, 0644);
    CHK(fd);
  };

 private:
  int fd;
  std::mutex mutex;
  std::shared_ptr<HeartbeatClient> heartbeat_client;

  Status Write(ServerContext *context, const hadev::WriteRequest *req,
               hadev::WriteReply *reply) {
    assert(req->data().length() == BLOCK_SIZE);
    mutex.lock();
    CHK(lseek(fd, req->addr(), SEEK_SET));
    assert(write(fd, req->data().c_str(), BLOCK_SIZE) != BLOCK_SIZE);
    CHK(fsync(fd));
    mutex.unlock();
    heartbeat_client->Write(req->addr(), req->data());
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
