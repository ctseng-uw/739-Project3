#include "heartbeat.h"

#include <fcntl.h>

/* ----------------- Heartbeat Server ----------------- */
HeartbeatServiceImpl::HeartbeatServiceImpl(std::shared_ptr<bool> i_am_primary)
    : i_am_primary(i_am_primary) {
  fd = open(DEVICE, O_CREAT | O_RDWR, 0644);
  assert(fd >= 0);
}

grpc::Status HeartbeatServiceImpl::RepliWrite(grpc::ServerContext *context,
                                              const hadev::Request *req,
                                              hadev::Reply *reply) {
  if (req->has_data() && req->has_addr()) {
    lseek(fd, req->addr(), SEEK_SET);
    write(fd, req->data().c_str(), 4096);
  }

  reply->set_yeah(true);
  return grpc::Status::OK;
}

grpc::Status HeartbeatServiceImpl::is_primary(grpc::ServerContext *context,
                                              const hadev::Blank *req,
                                              hadev::Reply *reply) {
  reply->set_yeah(*i_am_primary);
  return grpc::Status::OK;
}

/* ----------------- Heartbeat Client Exception ----------------- */
RPCFailException::RPCFailException(grpc::Status status) : status(status) {}

const char *RPCFailException::what() const throw() {
  std::stringstream ss;
  ss << "gRPC status " << status.error_code() << ": " << status.error_message();
  return ss.str().c_str();  // TODO: fix
}

/* ----------------- Heartbeat Client  ----------------- */
HeartbeatClient::HeartbeatClient(std::shared_ptr<grpc::Channel> channel)
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
    throw new RPCFailException(status);
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
    throw new RPCFailException(status);
  }
  return reply.yeah();
}
