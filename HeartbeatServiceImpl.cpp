#include "HeartbeatServiceImpl.hpp"

#include <fcntl.h>

#include "macro.h"
#include "server.h"

HeartbeatServiceImpl::HeartbeatServiceImpl(
    std::shared_ptr<ServerState> server_state_ptr,
    std::shared_ptr<struct timespec> last_heartbeat, int node_num)
    : server_state_ptr(server_state_ptr),
      last_heartbeat(last_heartbeat),
      node_num(node_num) {
  fd = open(DEVICE, O_CREAT | O_RDWR, 0644);
  assert(fd >= 0);
  clock_gettime(CLOCK_MONOTONIC, &*last_heartbeat);
}

grpc::Status HeartbeatServiceImpl::RepliWrite(grpc::ServerContext *context,
                                              const hadev::Request *req,
                                              hadev::Reply *reply) {
  ServerState state = *server_state_ptr;
  switch (state) {
    case INIT:
      // this should be a heartbeat, not a real write request
      assert(!req->has_addr() && !req->has_data());
      puts("HBServer: INIT to BACKUP");
      *server_state_ptr = BACKUP;
      // HeartbeatClint::Start() will figure out server_state_ptr immediately.
      std::cout << "HeartbeatServiceImpl: turned to BACKUP\n";
      clock_gettime(CLOCK_MONOTONIC, &*last_heartbeat);

      reply->set_yeah(true);
      return grpc::Status::OK;
    case BACKUP:
      // Normal case: refresh timeout and optionally write data
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
    case PRIMARY:
      // Happens when both nodes think they are PRIMARY.
      puts("HeartbeatServiceImpl: both nodes think they're PRIMARY");
      puts("HBServer: PRIMARY to INIT");
      *server_state_ptr = INIT;
      // throw std::runtime_error(
      // "HeartbeatServiceImpl: receive RepliWrite when state==PRIMARY");
      return grpc::Status(grpc::StatusCode::ABORTED,
                          "Both of us think we are PRIMARY. Switched to INIT");
  }
}

grpc::Status HeartbeatServiceImpl::GetState(grpc::ServerContext *context,
                                            const hadev::State *req,
                                            hadev::State *reply) {
  std::cout << "server: GetState is called\n";
  switch (*server_state_ptr) {
    case INIT:
      if (node_num)
        puts("HBService::GetState: INIT to BACKUP");
      else
        puts("HBService::GetState: INIT to PRIMARY");

      *server_state_ptr = (node_num ? BACKUP : PRIMARY);
      // HeartbeatClint::Start() will figure out server_state_ptr immediately.
      break;
    case BACKUP:
      puts("HBService::GetState: BACKUP to PRIMARY");
      *server_state_ptr = PRIMARY;
      // TODO: wakeup HeartbeatClient and make timeout countdown thread sleep.
      break;
    case PRIMARY:
      break;
  }

  reply->set_state(*server_state_ptr);
  return grpc::Status::OK;
}
