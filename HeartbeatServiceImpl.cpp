#include "HeartbeatServiceImpl.hpp"

#include <fcntl.h>

#include "macro.h"
#define DEVICE "./fake_device.bin"

HeartbeatServiceImpl::HeartbeatServiceImpl(std::shared_ptr<int> i_am_primary,
                                std::shared_ptr<struct timespec> last_heartbeat)
    : i_am_primary(i_am_primary), last_heartbeat(last_heartbeat) {
  fd = open(DEVICE, O_CREAT | O_RDWR, 0644);
  assert(fd >= 0);
  clock_gettime(CLOCK_MONOTONIC, &*last_heartbeat);
}

grpc::Status HeartbeatServiceImpl::RepliWrite(grpc::ServerContext *context,
                                              const hadev::Request *req,
                                              hadev::Reply *reply) {
  if (req->has_data() && req->has_addr()) {
    std::lock_guard<std::mutex> lg(mutex);
    assert(req->data().length() == BLOCK_SIZE);
    lseek(fd, req->addr(), SEEK_SET);
    write(fd, req->data().c_str(), BLOCK_SIZE);
    fsync(fd);
  }
  clock_gettime(CLOCK_MONOTONIC, &*last_heartbeat);

  reply->set_yeah(true);
  return grpc::Status::OK;
}

grpc::Status HeartbeatServiceImpl::is_primary(grpc::ServerContext *context,
                                              const hadev::Blank *req,
                                              hadev::ReplyState *reply) {
  reply->set_state(*i_am_primary);
  return grpc::Status::OK;
}
