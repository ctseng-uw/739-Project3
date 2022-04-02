#include "HeartbeatServiceImpl.hpp"

#include <fcntl.h>

#include "TimeoutWatcher.cpp"
#include "macro.h"
HeartbeatServiceImpl::HeartbeatServiceImpl(
    std::shared_ptr<bool> i_am_primary, std::shared_ptr<TimeoutWatcher> watcher,
    const int my_node_number)
    : i_am_primary(i_am_primary),
      watcher(watcher),
      my_node_number(my_node_number) {
  fd = open(DEVICE, O_CREAT | O_RDWR, 0644);
  assert(fd >= 0);
}

grpc::Status HeartbeatServiceImpl::RepliWrite(grpc::ServerContext *context,
                                              const hadev::Request *req,
                                              hadev::Reply *reply) {
  if (req->has_data() && req->has_addr()) {
    puts("Get Write request");
  } else {
    puts("Get heartbeat");
  }

  // if I am Primary, Check who should be the primary
  if (*i_am_primary) {
    puts("Double primary");
    // Check who wins to be the real primary
    if (my_node_number == 1) {
      puts("PRIMARY to BACKUP");
      *i_am_primary = false;
    } else {
      // I am the primary. refuse to write
      puts("I'm PRIMARY. Refuse to change (and write)");
      reply->set_i_am_primary(*i_am_primary);
      return grpc::Status::OK;
    }
  }

  // If this is a write request and I am backup, I write
  if (req->has_data() && req->has_addr()) {
    assert(*i_am_primary == false);
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
