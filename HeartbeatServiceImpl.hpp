#include "includes/heartbeat.grpc.pb.h"
#include "includes/heartbeat.pb.h"
#include "time.h"

class HeartbeatServiceImpl final : public hadev::Heartbeat::Service {
 public:
  HeartbeatServiceImpl(std::shared_ptr<int> i_am_primary,
                      std::shared_ptr<struct timespec> last_heartbeat);
  grpc::Status RepliWrite(grpc::ServerContext *context,
                          const hadev::Request *req, hadev::Reply *reply);
  grpc::Status is_primary(grpc::ServerContext *context, const hadev::Blank *req,
                          hadev::ReplyState *reply);

 private:
  int fd;
  std::shared_ptr<int> i_am_primary;
  std::mutex mutex;
  std::shared_ptr<struct timespec> last_heartbeat;
};
