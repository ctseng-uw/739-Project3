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
#define DEVICE "/users/c5h11oh/739-Project3/fake_device"

/* ----------------- Heartbeat Server ----------------- */

class HeartbeatServer final : public hafs::Heartbeat::Service {
 public:
  HeartbeatServer(bool *i_am_primary);
  grpc::Status heartbeat(grpc::ServerContext *context, const hafs::Request *req,
                         hafs::Reply *reply);
  grpc::Status is_primary(grpc::ServerContext *context, const hafs::Blank *req,
                          hafs::Reply *reply);

 private:
  int fd;
  bool *i_am_primary;
};

/* ----------------- Heartbeat Client ----------------- */

class RPCFailException : public std::exception {
 public:
  RPCFailException(grpc::Status status);
  virtual const char *what() const throw();

 private:
  grpc::Status status;
};

class HeartbeatClient {
 public:
  HeartbeatClient(std::shared_ptr<grpc::Channel> channel);
  bool is_primary();
  bool heartbeat();
  bool write(uint64_t addr, std::string data);

 private:
  std::unique_ptr<hafs::Heartbeat::Stub> stub_;
};
