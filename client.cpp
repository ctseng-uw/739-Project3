#include <iostream>

#include "BlockStoreClient.cpp"

int main(int argc, char **argv) {
  BlockStoreClient hadev(argv[1][0] == '0' ? "10.10.1.1:50051"
                                           : "10.10.1.2:50051");
  if (argv[2][0] == 'r') {
    std::cout << hadev.Read(std::atoi(argv[3]));
  } else {
    hadev.Write(std::atoi(argv[3]), argv[4]);
  }
}
