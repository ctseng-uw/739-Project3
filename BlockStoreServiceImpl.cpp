#include <fcntl.h>
#include <unistd.h>

#include <mutex>

#include "HeartbeatClient.cpp"
#include "includes/blockstore.grpc.pb.h"
#include "includes/blockstore.pb.h"
#include "macro.h"

using grpc::ServerContext;
using grpc::Status;
using grpc::StatusCode;

class BlockStoreServiceImpl final : public hadev::BlockStore::Service {
 public:
  explicit BlockStoreServiceImpl(
      std::shared_ptr<ServerState> server_state,
      std::shared_ptr<HeartbeatClient> heartbeat_client)
      : server_state(server_state), heartbeat_client(heartbeat_client) {
    fd = open(DEVICE, O_CREAT | O_RDWR, 0644);
    CHK(fd);
  };

 private:
  int fd;
  std::mutex mutex;
  std::shared_ptr<HeartbeatClient> heartbeat_client;
  std::shared_ptr<ServerState> server_state;

  Status Write(ServerContext *context, const hadev::WriteRequest *req,
               hadev::WriteReply *reply) {
    // Accept client requests only when status == PRIMARY
    if (*server_state != PRIMARY) {
      std::string state = (*server_state == INIT ? "INIT" : "BACKUP");
      return Status(StatusCode::FAILED_PRECONDITION,
                    "Request rejected: I am " + state);
    }

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
    // Accept client requests only when status == PRIMARY
    if (*server_state != PRIMARY) {
      std::string state = (*server_state == INIT ? "INIT" : "BACKUP");
      return Status(StatusCode::FAILED_PRECONDITION,
                    "Request rejected: I am " + state);
    }

    lseek(fd, req->addr(), SEEK_SET);
    char buf[BLOCK_SIZE];
    read(fd, buf, BLOCK_SIZE);
    reply->set_data(std::string(buf, BLOCK_SIZE));
    reply->set_ret(0);
    return Status::OK;
  }
};
