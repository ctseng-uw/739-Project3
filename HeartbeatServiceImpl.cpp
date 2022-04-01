#include "HeartbeatServiceImpl.hpp"

#include <fcntl.h>

#include "TimeoutWatcher.cpp"
#include "macro.h"
HeartbeatServiceImpl::HeartbeatServiceImpl(
    std::shared_ptr<bool> i_am_primary, std::shared_ptr<TimeoutWatcher> watcher)
    : i_am_primary(i_am_primary), watcher(watcher) {
  fd = open(DEVICE, O_CREAT | O_RDWR, 0644);
  assert(fd >= 0);
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

  reply->set_i_am_primary(*i_am_primary);
  watcher->NotifyHeartBeat();
  return grpc::Status::OK;
}
