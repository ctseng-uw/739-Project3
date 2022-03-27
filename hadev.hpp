#include <fcntl.h>
#include <grpcpp/client_context.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/support/status.h>
#include <signal.h>
#include <sys/stat.h>

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>

#include "includes/hello.grpc.pb.h"
#include "includes/hello.pb.h"

using grpc::ClientContext;

class GRPCClient {
 public:
  GRPCClient(std::shared_ptr<grpc::Channel> channel)
      : stub_(hadev::gRPCService::NewStub(channel)) {}

  int Write(const int64_t addr, const std::string &data) {
    hadev::WriteRequest request;
    request.set_data(data);
    ClientContext context;
    hadev::WriteReply reply;
    auto status = stub_->Write(&context, request, &reply);
    if (!status.ok()) {
      return -1;
    }
    return 0;
  }

  std::string Read(const int64_t addr) {
    hadev::ReadRequest request;
    request.set_addr(addr);
    ClientContext context;
    hadev::ReadReply reply;
    auto status = stub_->Read(&context, request, &reply);
    if (!status.ok()) {
      return nullptr;
    }
    return reply.data();
  }

 private:
  std::unique_ptr<hadev::gRPCService::Stub> stub_;
};

class HadevClient {
 public:
  HadevClient() {
    const std::string target_str = "0.0.0.0:50051";
    grpc::ChannelArguments ch_args;

    ch_args.SetMaxReceiveMessageSize(INT_MAX);
    ch_args.SetMaxSendMessageSize(INT_MAX);

    gRPCClient = std::make_unique<GRPCClient>((grpc::CreateCustomChannel(
        target_str, grpc::InsecureChannelCredentials(), ch_args)));
  }
  auto Write(const auto... args) { return gRPCClient->Write(args...); }
  auto Read(const auto... args) { return gRPCClient->Read(args...); }

 private:
  std::unique_ptr<GRPCClient> gRPCClient;
};
