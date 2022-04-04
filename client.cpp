#include <cctype>
#include <iostream>

#include "BlockStoreClient.cpp"

inline void error() {
  std::cerr << "Error input\n";
  exit(1);
}

int main(int argc, char **argv) {
  int server_id;
  int64_t addr;
  std::string data;
  bool is_read;
  bool designated_server;
  if (argc < 3) {
    error();
  } else if (argc == 3) {
    // r <addr>
    if (argv[1][0] != 'r') error();
    server_id = 0;
    addr = std::atoi(argv[2]);
    is_read = true;
    designated_server = false;
  } else if (argc == 4 && std::isdigit(argv[2][0])) {
    // w <addr> <content>
    if (argv[1][0] != 'w') error();
    server_id = 0;
    addr = std::atoi(argv[2]);
    data = std::string(argv[3]);
    is_read = false;
    designated_server = false;
  } else if (argc == 4 && argv[2][0] == 'r') {
    // <server_id> r <addr>
    server_id = std::atoi(argv[1]);
    addr = std::atoi(argv[3]);
    is_read = true;
    designated_server = true;
  } else if (argc == 4) {
    error();
  } else if (argc == 5) {
    // <server_id> w <addr> <content>
    if (argv[2][0] != 'w') error();
    server_id = std::atoi(argv[1]);
    addr = std::atoi(argv[3]);
    data = std::string(argv[4]);
    is_read = false;
    designated_server = true;
  } else {
    error();
  }

  BlockStoreClient hadev(server_id, designated_server);
  if (is_read) {
    std::cout << hadev.Read(addr);
  } else {
    return hadev.Write(addr, data);
  }
}
