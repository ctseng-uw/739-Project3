#include "HeartbeatClient.cpp"
#include "TimeoutWatcher.cpp"
#include "includes/heartbeat.grpc.pb.h"
#include "includes/heartbeat.pb.h"

class HeartbeatServiceImpl final : public hadev::Heartbeat::Service {
 public:
  HeartbeatServiceImpl(std::shared_ptr<bool> i_am_primary,
                       std::shared_ptr<TimeoutWatcher> watcher,
                       const int my_node_number, std::shared_ptr<HeartbeatClient> hb_client);
  grpc::Status RepliWrite(grpc::ServerContext *context,
                          const hadev::Request *req, hadev::Reply *reply);

 private:
  int fd;
  std::shared_ptr<bool> i_am_primary;
  std::mutex mutex;
  std::shared_ptr<TimeoutWatcher> watcher;
  std::shared_ptr<HeartbeatClient> hb_client;
  const int my_node_number;
};
