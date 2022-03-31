#include <fcntl.h>
#include <unistd.h>

#include <mutex>

#include "HeartbeatClient.cpp"
#include "includes/blockstore.grpc.pb.h"
#include "includes/blockstore.pb.h"
#include "macro.h"

using grpc::ServerContext;
using grpc::Status;

class BlockStoreServiceImpl final : public hadev::BlockStore::Service {
 public:
  explicit BlockStoreServiceImpl(
      std::shared_ptr<HeartbeatClient> heartbeat_client)
      : heartbeat_client(heartbeat_client) {
    fd = open(DEVICE, O_CREAT | O_RDWR, 0644);
    CHK(fd);
  };

 private:
  int fd;
  std::mutex mutex;
  std::shared_ptr<HeartbeatClient> heartbeat_client;

  Status Write(ServerContext *context, const hadev::WriteRequest *req,
               hadev::WriteReply *reply) {
    assert(req->data().length() == BLOCK_SIZE);
    {
      std::lock_guard<std::mutex> lg(mutex);
      CHK(lseek(fd, req->addr(), SEEK_SET));
      assert(write(fd, req->data().c_str(), BLOCK_SIZE) == BLOCK_SIZE);
      CHK(fsync(fd));
      heartbeat_client->Write(req->addr(), req->data());
    }
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
