#include "HeartbeatServiceImpl.hpp"

#include <fcntl.h>

#include "macro.h"
#include "server.h"

HeartbeatServiceImpl::HeartbeatServiceImpl(
    std::shared_ptr<ServerState> server_state,
    std::shared_ptr<struct timespec> last_heartbeat)
    : server_state(server_state), last_heartbeat(last_heartbeat) {
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

grpc::Status HeartbeatServiceImpl::GetState(grpc::ServerContext *context,
                                            const hadev::Blank *req,
                                            hadev::State *reply) {
  reply->set_state(*server_state);
  return grpc::Status::OK;
}
