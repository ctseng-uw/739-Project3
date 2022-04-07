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
#include <vector>

#include "includes/blockstore.grpc.pb.h"
#include "includes/blockstore.pb.h"
#include "macro.h"

using grpc::ClientContext;
using namespace std::chrono_literals;

class BlockStoreClient {
 public:
  BlockStoreClient(int current_server = 0, bool designated_server = false)
      : current_server(current_server), designated_server(designated_server) {
    server_ip = {
        {{{"node0:" + PORT,
           "node0.hadev3.advosuwmadison-pg0.wisc.cloudlab.us:" + PORT}},
         {{"node1:" + PORT,
           "node1.hadev3.advosuwmadison-pg0.wisc.cloudlab.us:" + PORT}}}};

    current_iface = 0;
    grpc::ChannelArguments ch_args;

    ch_args.SetMaxReceiveMessageSize(INT_MAX);
    ch_args.SetMaxSendMessageSize(INT_MAX);

    stub_ = hadev::BlockStore::NewStub((grpc::CreateCustomChannel(
        server_ip[current_server][current_iface],
        grpc::InsecureChannelCredentials(), ch_args)));
  }

  int Write(const int64_t addr, const std::string &data) {
    while (true) {
      std::string copy(data, 0, 4096);
      copy.resize(4096);
      hadev::WriteRequest request;
      request.set_addr(addr);
      request.set_data(copy);
      ClientContext context;
      hadev::WriteReply reply;
      context.set_wait_for_ready(true);
      context.set_deadline(std::chrono::system_clock::now() + 4s);
      auto status = stub_->Write(&context, request, &reply);
      if (!status.ok()) {
        puts("failed once");
        ChangeIface();
        ClientContext context;
        context.set_wait_for_ready(true);
        context.set_deadline(std::chrono::system_clock::now() + 4s);
        status = stub_->Write(&context, request, &reply);
      }
      if (!status.ok() ||
          reply.ret() == 1) {  // ret() == 1 means server is backup
        if (!designated_server) {
          std::cout << "ChangeServer in Write" << std::endl;
          ChangeServer();
          continue;
        }
        std::cerr << (status.ok() ? "This server is backup"
                                  : "Fail to call server");
        return -1;
      }
      std::cerr << "Write to node" << current_server;
      return 0;
    }
  }

  std::string Read(const int64_t addr) {
    while (true) {
      hadev::ReadRequest request;
      request.set_addr(addr);
      ClientContext context;
      context.set_wait_for_ready(true);
      context.set_deadline(std::chrono::system_clock::now() + 4s);
      hadev::ReadReply reply;
      auto status = stub_->Read(&context, request, &reply);
      if (!status.ok()) {
        ChangeIface();
        ClientContext context;
        context.set_wait_for_ready(true);
        context.set_deadline(std::chrono::system_clock::now() + 4s);
        status = stub_->Read(&context, request, &reply);
      }
      if (!status.ok() ||
          reply.ret() == 1) {  // ret() == 1 means server is backup
        if (!designated_server) {
          // std::cout << "ChangeServer in Read" << std::endl;
          ChangeServer();
          continue;
        }
        std::cerr << (status.ok() ? "This server is backup"
                                  : "Fail to call server");
        return nullptr;
      }
      std::cerr << "Read from node" << current_server;
      return reply.data();
    }
  }

  ~BlockStoreClient() { stub_.release(); }

 private:
  std::unique_ptr<hadev::BlockStore::Stub> stub_;
  std::array<std::array<std::string, 2>, 2> server_ip;
  int current_server, current_iface;
  bool designated_server;

  void ChangeServer() {
    current_server = 1 - current_server;

    stub_.release();

    grpc::ChannelArguments ch_args;

    ch_args.SetMaxReceiveMessageSize(INT_MAX);
    ch_args.SetMaxSendMessageSize(INT_MAX);

    stub_ = hadev::BlockStore::NewStub((grpc::CreateCustomChannel(
        server_ip[current_server][current_iface],
        grpc::InsecureChannelCredentials(), ch_args)));
  }

  void ChangeIface() {
    current_iface = 1 - current_iface;

    stub_.release();

    grpc::ChannelArguments ch_args;

    ch_args.SetMaxReceiveMessageSize(INT_MAX);
    ch_args.SetMaxSendMessageSize(INT_MAX);

    stub_ = hadev::BlockStore::NewStub((grpc::CreateCustomChannel(
        server_ip[current_server][current_iface],
        grpc::InsecureChannelCredentials(), ch_args)));
  }
};
