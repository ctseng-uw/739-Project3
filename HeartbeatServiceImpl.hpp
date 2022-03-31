#include <time.h>

#include "includes/heartbeat.grpc.pb.h"
#include "includes/heartbeat.pb.h"
#include "server.h"

class HeartbeatServiceImpl final : public hadev::Heartbeat::Service {
 public:
  HeartbeatServiceImpl(std::shared_ptr<ServerState> server_state,
                       std::shared_ptr<struct timespec> last_heartbeat);
  grpc::Status RepliWrite(grpc::ServerContext *context,
                          const hadev::Request *req, hadev::Reply *reply);
  grpc::Status GetState(grpc::ServerContext *context, const hadev::Blank *req,
                        hadev::State *reply);

 private:
  int fd;
  std::shared_ptr<ServerState> server_state;
  std::mutex mutex;
  std::shared_ptr<struct timespec> last_heartbeat;
};
