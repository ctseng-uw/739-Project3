#include <iostream>

#include "BlockStoreClient.cpp"

int main(int argc, char **argv) {
  BlockStoreClient hadev;
  if (argv[1][0] == 'r') {
    std::cout << hadev.Read(std::atoi(argv[2]));
  } else {
    hadev.Write(std::atoi(argv[2]), argv[3]);
  }
}
