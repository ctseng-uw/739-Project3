#pragma once
#include <exception>

#include "includes/heartbeat.grpc.pb.h"
#include "includes/heartbeat.pb.h"

class RPCFailException : public std::runtime_error {
 public:
  RPCFailException(grpc::Status status);
};

class HeartbeatClient {
 public:
  HeartbeatClient(const std::shared_ptr<grpc::ChannelInterface>& channel);
  bool is_primary();
  bool BeatHeart();
  bool Write(uint64_t addr, std::string data);

 private:
  std::unique_ptr<hadev::Heartbeat::Stub> stub_;
};
