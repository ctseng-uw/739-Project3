#include <iostream>

#include "BlockStoreClient.cpp"

int main(int argc, char **argv) {
  BlockStoreClient hadev;
  hadev.Write(0, argv[1]);

  std::cout << hadev.Read(0) << std::endl;
}
