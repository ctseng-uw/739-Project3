#include <iostream>
#include "heartbeat.h"

int main(int argc, char** argv) {
    bool i_am_primary = false;
    HeartbeatServer heartbeatServer(&i_am_primary);

    const std::string target_str = "0.0.0.0:50051";
    grpc::ChannelArguments ch_args;

    ch_args.SetMaxReceiveMessageSize(INT_MAX);
    ch_args.SetMaxSendMessageSize(INT_MAX);

    HeartbeatClient heartbeatClient(grpc::CreateCustomChannel(
        target_str, grpc::InsecureChannelCredentials(), ch_args));

    try
    {
        std::cout << heartbeatClient.is_primary() << std::endl;
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
    return 0;
}