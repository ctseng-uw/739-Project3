#include "heartbeat.h"

/* ----------------- Heartbeat Server ----------------- */
HeartbeatServer::HeartbeatServer(bool *i_am_primary)
    : i_am_primary(i_am_primary) {
  fd = open(DEVICE, O_RDWR);
  assert(fd >= 0);
}

grpc::Status HeartbeatServer::heartbeat(grpc::ServerContext *context,
                                        const hafs::Request *req,
                                        hafs::Reply *reply) {
  if (req->is_write()) {
    lseek(fd, req->addr(), SEEK_SET);
    write(fd, req->data().c_str(), 4096);
  }

  reply->set_yeah(true);
  return grpc::Status::OK;
}

grpc::Status HeartbeatServer::is_primary(grpc::ServerContext *context,
                                         const hafs::Blank *req,
                                         hafs::Reply *reply) {
  reply->set_yeah(*i_am_primary);
  return grpc::Status::OK;
}

/* ----------------- Heartbeat Client Exception ----------------- */
RPCFailException::RPCFailException(grpc::Status status) : status(status) {}

const char *RPCFailException::what() const throw() {
  std::stringstream ss;
  ss << "gRPC status " << status.error_code() << ": " << status.error_message();
  return ss.str().c_str();
}

/* ----------------- Heartbeat Client  ----------------- */
HeartbeatClient::HeartbeatClient(std::shared_ptr<grpc::Channel> channel)
    : stub_(hafs::Heartbeat::NewStub(channel)) {}

bool HeartbeatClient::is_primary() {
  hafs::Blank request;
  grpc::ClientContext context;
  hafs::Reply reply;
  auto status = stub_->is_primary(&context, request, &reply);
  if (!status.ok()) {
    throw RPCFailException(status);
    // throw std::exception();
  }
  return reply.yeah();
}

bool HeartbeatClient::heartbeat() {
  hafs::Request request;
  grpc::ClientContext context;
  hafs::Reply reply;

  request.set_is_write(false);
  grpc::Status status = stub_->heartbeat(&context, request, &reply);
  if (!status.ok()) {
    throw new RPCFailException(status);
  }
  return reply.yeah();
}

bool HeartbeatClient::write(uint64_t addr, std::string data) {
  hafs::Request request;
  grpc::ClientContext context;
  hafs::Reply reply;

  request.set_is_write(true);
  request.set_addr(addr);
  request.set_data(data);
  grpc::Status status = stub_->heartbeat(&context, request, &reply);
  if (!status.ok()) {
    throw new RPCFailException(status);
  }
  return reply.yeah();
}
