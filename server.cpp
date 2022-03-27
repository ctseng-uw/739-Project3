#include "includes/hello.grpc.pb.h"
#include "includes/hello.pb.h"

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/server_context.h>
#include <grpcpp/support/status.h>
#include <signal.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

constexpr unsigned long BLOCK_SIZE = 4096;

class gRPCServiceImpl final : public hafs::gRPCService::Service {
public:
  gRPCServiceImpl() { fd = open("server_device.bin", O_CREAT | O_RDWR, 0644); };

private:
  int fd;

  Status Write(ServerContext *context, const hafs::WriteRequest *req,
               hafs::WriteReply *reply) {
    lseek(fd, req->addr(), SEEK_SET);
    errno = 0;
    write(fd, req->data().c_str(), std::min(BLOCK_SIZE, req->data().length()));
    perror("write");
    fsync(fd);
    reply->set_ret(0);
    return Status::OK;
  }

  Status Read(ServerContext *context, const hafs::ReadRequest *req,
              hafs::ReadReply *reply) {
    lseek(fd, req->addr(), SEEK_SET);
    char buf[BLOCK_SIZE];
    read(fd, buf, BLOCK_SIZE);
    reply->set_data(buf);
    reply->set_ret(0);
    return Status::OK;
  }
};

int main() {
  std::string server_address("localhost:50051");
  gRPCServiceImpl service;
  grpc::ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.SetMaxSendMessageSize(INT_MAX);
  builder.SetMaxReceiveMessageSize(INT_MAX);
  builder.SetMaxMessageSize(INT_MAX);

  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  // Finally assemble the server.
  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}
