#include "HeartbeatServiceImpl.hpp"

#include <fcntl.h>

#include "macro.h"
#define DEVICE "./fake_device.bin"

HeartbeatServiceImpl::HeartbeatServiceImpl(std::shared_ptr<bool> i_am_primary)
    : i_am_primary(i_am_primary) {
  fd = open(DEVICE, O_CREAT | O_RDWR, 0644);
  assert(fd >= 0);
}

grpc::Status HeartbeatServiceImpl::RepliWrite(grpc::ServerContext *context,
                                              const hadev::Request *req,
                                              hadev::Reply *reply) {
  if (req->has_data() && req->has_addr()) {
    assert(req->data().length() == BLOCK_SIZE);
    lseek(fd, req->addr(), SEEK_SET);
    write(fd, req->data().c_str(), BLOCK_SIZE);
    fsync(fd);
  }

  reply->set_yeah(true);
  return grpc::Status::OK;
}

grpc::Status HeartbeatServiceImpl::is_primary(grpc::ServerContext *context,
                                              const hadev::Blank *req,
                                              hadev::Reply *reply) {
  reply->set_yeah(*i_am_primary);
  return grpc::Status::OK;
}
