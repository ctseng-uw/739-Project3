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

#include "includes/blockstore.grpc.pb.h"
#include "includes/blockstore.pb.h"
#include "macro.h"

using grpc::ClientContext;

class BlockStoreClient {
 public:
  BlockStoreClient(std::string server_name = "node1") {
    grpc::ChannelArguments ch_args;

    ch_args.SetMaxReceiveMessageSize(INT_MAX);
    ch_args.SetMaxSendMessageSize(INT_MAX);

    stub_ = hadev::BlockStore::NewStub((grpc::CreateCustomChannel(
        (server_name + ":" + std::to_string(PORT)),
        grpc::InsecureChannelCredentials(), ch_args)));
  }

  int Write(const int64_t addr, const std::string &data) {
    std::string copy(data, 0, 4096);
    copy.resize(4096);
    hadev::WriteRequest request;
    request.set_addr(addr);
    request.set_data(copy);
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

  ~BlockStoreClient() { stub_.release(); }

 private:
  std::unique_ptr<hadev::BlockStore::Stub> stub_;
};
