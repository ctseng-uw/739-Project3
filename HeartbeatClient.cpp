#include "HeartbeatClient.hpp"

/* ----------------- Heartbeat Client Exception ----------------- */
RPCFailException::RPCFailException(grpc::Status status)
    : std::runtime_error("gRPC status " + std::to_string(status.error_code()) +
                         ": " + status.error_message()) {}

/* ----------------- Heartbeat Client  ----------------- */
HeartbeatClient::HeartbeatClient(
    const std::shared_ptr<grpc::ChannelInterface>& channel)
    : stub_(hadev::Heartbeat::NewStub(channel)) {}

bool HeartbeatClient::is_primary() {
  hadev::Blank request;
  grpc::ClientContext context;
  hadev::Reply reply;
  auto status = stub_->is_primary(&context, request, &reply);
  if (!status.ok()) {
    throw RPCFailException(status);
    // throw std::exception();
  }
  return reply.yeah();
}

bool HeartbeatClient::BeatHeart() {
  hadev::Request request;
  grpc::ClientContext context;
  hadev::Reply reply;

  grpc::Status status = stub_->RepliWrite(&context, request, &reply);
  if (!status.ok()) {
    throw RPCFailException(status);
  }
  return reply.yeah();
}

bool HeartbeatClient::Write(uint64_t addr, std::string data) {
  hadev::Request request;
  grpc::ClientContext context;
  hadev::Reply reply;

  request.set_addr(addr);
  request.set_data(data);
  grpc::Status status = stub_->RepliWrite(&context, request, &reply);
  if (!status.ok()) {
    throw RPCFailException(status);
  }
  return reply.yeah();
}
