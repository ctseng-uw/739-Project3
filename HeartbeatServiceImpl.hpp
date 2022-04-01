#include "TimeoutWatcher.cpp"
#include "includes/heartbeat.grpc.pb.h"
#include "includes/heartbeat.pb.h"

class HeartbeatServiceImpl final : public hadev::Heartbeat::Service {
 public:
  HeartbeatServiceImpl(std::shared_ptr<bool> i_am_primary,
                       std::shared_ptr<TimeoutWatcher> watcher);
  grpc::Status RepliWrite(grpc::ServerContext *context,
                          const hadev::Request *req, hadev::Reply *reply);

 private:
  int fd;
  std::shared_ptr<bool> i_am_primary;
  std::mutex mutex;
  std::shared_ptr<TimeoutWatcher> watcher;
};
