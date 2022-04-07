#include "HeartbeatServiceImpl.hpp"

#include <fcntl.h>

#include "TimeoutWatcher.cpp"
#include "macro.h"
HeartbeatServiceImpl::HeartbeatServiceImpl(
    std::shared_ptr<bool> i_am_primary, std::shared_ptr<TimeoutWatcher> watcher,
    const int my_node_number, std::shared_ptr<HeartbeatClient> hb_client)
    : i_am_primary(i_am_primary),
      watcher(watcher),
      my_node_number(my_node_number),
      hb_client(hb_client) {
  fd = open(DEVICE, O_CREAT | O_RDWR, 0644);
  assert(fd >= 0);
}

grpc::Status HeartbeatServiceImpl::RepliWrite(grpc::ServerContext *context,
                                              const hadev::Request *req,
                                              hadev::Reply *reply) {
  bool write_req = req->has_data() && req->has_addr();
  std::cout << (write_req ? "Get Write request" : "Get heartbeat") << std::endl;

  // if I am Primary, Check who should be the primary
  if (*i_am_primary) {
    std::cout << "Double primary" << std::endl;
    // Check who wins to be the real primary
    // Stuck if both server logs are non-empty
    if ((write_req || my_node_number == 1) && hb_client->LogEmpty()) {
      std::cout << "PRIMARY to BACKUP" << std::endl;
      *i_am_primary = false;
    } else {
      // I am the primary. refuse to write
      std::cout << "I'm PRIMARY. Refuse to change (and write)" << std::endl;
      reply->set_i_am_primary(*i_am_primary);
      return grpc::Status::OK;
    }
  }

  // If this is a write request and I am backup, I write
  if (req->has_data() && req->has_addr()) {
    assert(*i_am_primary == false);
    std::lock_guard<std::mutex> lg(mutex);
    assert(req->data().length() == BLOCK_SIZE);
    CHK(lseek(fd, req->addr(), SEEK_SET));
    assert(write(fd, req->data().c_str(), BLOCK_SIZE) == BLOCK_SIZE);
    CHK(fsync(fd));
  }
  reply->set_i_am_primary(*i_am_primary);
  watcher->NotifyHeartBeat();
  return grpc::Status::OK;
}
