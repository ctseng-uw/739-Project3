#include <fcntl.h>
#include <unistd.h>

#include "build/protoh/blockstore.grpc.pb.h"
#include "build/protoh/blockstore.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

constexpr unsigned long BLOCK_SIZE = 4096;

class BlockStoreServiceImpl final : public hadev::BlockStore::Service {
 public:
  BlockStoreServiceImpl() {
    fd = open("server_device.bin", O_CREAT | O_RDWR, 0644);
    assert(fd >= 0);
  };

 private:
  int fd;
  std::array<char, 256> junk;

  Status Write(ServerContext *context, const hadev::WriteRequest *req,
               hadev::WriteReply *reply) {
    lseek(fd, req->addr(), SEEK_SET);
    int write_size = std::min(BLOCK_SIZE, req->data().length());
    write(fd, req->data().c_str(), write_size);
    write(fd, junk.data(), BLOCK_SIZE - write_size);
    fsync(fd);
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
