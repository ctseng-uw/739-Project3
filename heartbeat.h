#include <fcntl.h>
#include <grpcpp/client_context.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/support/status.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <exception>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "build/protoh/heartbeat.grpc.pb.h"
#include "build/protoh/heartbeat.pb.h"

// #define DEVICE "/dev/sda4"
#define DEVICE "./fake_device"

/* ----------------- Heartbeat Server ----------------- */

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

/* ----------------- Heartbeat Client ----------------- */

class RPCFailException : public std::runtime_error {
 public:
  RPCFailException(grpc::Status status);
};

class HeartbeatClient {
 public:
  HeartbeatClient(std::shared_ptr<grpc::Channel> channel);
  bool is_primary();
  bool BeatHeart();
  bool Write(uint64_t addr, std::string data);

 private:
  std::unique_ptr<hadev::Heartbeat::Stub> stub_;
};
