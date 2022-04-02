#include <iostream>

#include "BlockStoreClient.cpp"

int main(int argc, char **argv) {
  int server_id;
  int64_t addr;
  std::string data;
  bool is_read;
  bool designated_server;

  if (argc == 3) {
    server_id = 0;
    addr = std::atoi(argv[2]);
    is_read = true;
    designated_server = false;
  }
  else if (argc == 4) {
    if (argv[2][0] == 'r') {
      server_id = std::atoi(argv[1]);
      addr = std::atoi(argv[3]);
      is_read = true;
      designated_server = true;
    }
    else {
      server_id = 0;
      addr = std::atoi(argv[2]);
      data = std::string(argv[3]);
      is_read = false;
      designated_server = false;
    }
  }
  else if (argc == 5) {
    server_id = std::atoi(argv[1]);
    addr = std::atoi(argv[3]);
    data = std::string(argv[4]);
    is_read = false;
    designated_server = true;
  }

  BlockStoreClient hadev(server_id, designated_server);
  if (is_read) {
    std::cout << hadev.Read(addr);
  } else {
    hadev.Write(addr, data);
  }
}
