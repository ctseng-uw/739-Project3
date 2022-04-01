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
      std::shared_ptr<bool> i_am_primary,
      std::shared_ptr<HeartbeatClient> heartbeat_client)
      : heartbeat_client(heartbeat_client), i_am_primary(i_am_primary) {
    fd = open(DEVICE, O_CREAT | O_RDWR, 0644);
    CHK(fd);
  };

 private:
  int fd;
  std::mutex mutex;
  std::shared_ptr<bool> i_am_primary;
  std::shared_ptr<HeartbeatClient> heartbeat_client;

  Status Write(ServerContext *context, const hadev::WriteRequest *req,
               hadev::WriteReply *reply) {
    assert(req->data().length() == BLOCK_SIZE);
    if (*i_am_primary == false) {
      reply->set_ret(1);  // Saying I am backup
      return Status::OK;
    }
    {
      bool remote_write_succeed =
          heartbeat_client->Write(req->addr(), req->data());
      // write fails only when the remote compares node_number and
      // then still think it is the primary. we must be backup (node1) then.
      if (!remote_write_succeed) {
        *i_am_primary = false;
        reply->set_ret(1);  // Means I am backup;
        return Status::OK;
      }
      std::lock_guard<std::mutex> lg(mutex);
      CHK(lseek(fd, req->addr(), SEEK_SET));
      assert(write(fd, req->data().c_str(), BLOCK_SIZE) == BLOCK_SIZE);
      CHK(fsync(fd));
      reply->set_ret(0);
      return Status::OK;
    }
  }

  Status Read(ServerContext *context, const hadev::ReadRequest *req,
              hadev::ReadReply *reply) {
    if (*i_am_primary == false) {
      reply->set_ret(1);  // Saying I am backup
      return Status::OK;
    }
    lseek(fd, req->addr(), SEEK_SET);
    char buf[BLOCK_SIZE];
    read(fd, buf, BLOCK_SIZE);
    reply->set_data(std::string(buf, BLOCK_SIZE));
    reply->set_ret(0);
    return Status::OK;
  }
};
