#include <iostream>

#include "BlockStoreClient.cpp"

int main(int argc, char **argv) {
  BlockStoreClient hadev;
  hadev.Write(std::atoi(argv[1]), argv[2]);

  std::cout << hadev.Read(std::atoi(argv[1])) << std::endl;
}
