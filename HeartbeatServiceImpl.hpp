#include "includes/heartbeat.grpc.pb.h"
#include "includes/heartbeat.pb.h"

class HeartbeatServiceImpl final : public hadev::Heartbeat::Service {
 public:
  HeartbeatServiceImpl(std::shared_ptr<bool> i_am_primary);
  grpc::Status RepliWrite(grpc::ServerContext *context,
                          const hadev::Request *req, hadev::Reply *reply);
  grpc::Status is_primary(grpc::ServerContext *context, const hadev::Blank *req,
                          hadev::Reply *reply);

 private:
  int fd;
  std::shared_ptr<bool> i_am_primary;
};
