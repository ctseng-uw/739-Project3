#include <iostream>
#include <thread>
#include <unistd.h>
#include <stdlib.h>
#include "heartbeat.h"

bool i_am_primary = false;
int my_node_number;
const std::array<std::string, 2> LAN_ADDR{"10.10.1.1", "10.10.1.2"};
const std::string HEARTTBEATPORT = "53705";
const std::string BLOCKSTOREPORT = "50051";
const uint32_t TIMEOUTMS = 10;

void heartbeat_server() {
    HeartbeatServer heartbeatServer(&i_am_primary);

    const std::string target_str = std::string("0.0.0.0:") + HEARTTBEATPORT;
    grpc::ServerBuilder builder;
    // Listen on the given address without any authentication mechanism.
    builder.AddListeningPort(target_str, grpc::InsecureServerCredentials());
    builder.SetMaxSendMessageSize(INT_MAX);
    builder.SetMaxReceiveMessageSize(INT_MAX);
    builder.SetMaxMessageSize(INT_MAX);

    // Register "service" as the instance through which we'll communicate with
    // clients. In this case it corresponds to an *synchronous* service.
    builder.RegisterService(&heartbeatServer);
    // Finally assemble the server.
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << target_str << std::endl;

    // Wait for the server to shutdown. Note that some other thread must be
    // responsible for shutting down the server for this call to ever return.
    server->Wait();
}

void blockstore_server() {
    while (true)
        ;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: " << argv[0] << " <node_number, 0 or 1> <i_am_primary, 0 or 1>\n";
        exit(1);
    }
    my_node_number = atoi(argv[1]);
    i_am_primary = atoi(argv[2]);

    std::thread hb_server_thd(heartbeat_server);
    std::thread bs_server_thd(blockstore_server);

    // heartbeat_client: send heartbeat every 0.3 timeout
    grpc::ChannelArguments ch_args;
    ch_args.SetMaxReceiveMessageSize(INT_MAX);
    ch_args.SetMaxSendMessageSize(INT_MAX);

    HeartbeatClient heartbeatClient(grpc::CreateCustomChannel(
    LAN_ADDR[1 - my_node_number] + HEARTTBEATPORT, grpc::InsecureChannelCredentials(), ch_args));
    while (true) {
        while (i_am_primary) {
            try
            {
                std::cout << heartbeatClient.heartbeat() << std::endl;
                usleep(TIMEOUTMS * 300);
            }
            catch(const std::exception& e)
            {
                std::cerr << e.what() << '\n';
            }
        }
    }

    return 0;
}