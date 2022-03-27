#include <cstdint>
#include <fcntl.h>
#include <grpcpp/client_context.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/support/status.h>
#include <signal.h>
#include <sys/stat.h>

#include "includes/hello.grpc.pb.h"
#include "includes/hello.pb.h"
#include <iostream>
#include <memory>
#include <string>

using grpc::ClientContext;

class GRPCClient {
public:
  GRPCClient(std::shared_ptr<grpc::Channel> channel)
      : stub_(hafs::gRPCService::NewStub(channel)) {}

  int Write(int64_t addr, std::string &data) {
    hafs::WriteRequest request;
    request.set_data(data);
    ClientContext context;
    hafs::WriteReply reply;
    auto status = stub_->Write(&context, request, &reply);
    if (!status.ok()) {
      return -1;
    }
    return 0;
  }

  std::string Read(int64_t addr) {
    hafs::ReadRequest request;
    request.set_addr(addr);
    ClientContext context;
    hafs::ReadReply reply;
    auto status = stub_->Read(&context, request, &reply);
    if (!status.ok()) {
      return nullptr;
    }
    return reply.data();
  }

private:
  std::unique_ptr<hafs::gRPCService::Stub> stub_;
};

int main(int argc, char *argv[]) {
  const std::string target_str = "0.0.0.0:50051";
  grpc::ChannelArguments ch_args;

  ch_args.SetMaxReceiveMessageSize(INT_MAX);
  ch_args.SetMaxSendMessageSize(INT_MAX);

  GRPCClient gRPCClient(grpc::CreateCustomChannel(
      target_str, grpc::InsecureChannelCredentials(), ch_args));

  std::string apple{"apple"};
  std::cout << gRPCClient.Write(0, apple) << std::endl;
  std::cout << gRPCClient.Read(0) << std::endl;

  return 0;
}
