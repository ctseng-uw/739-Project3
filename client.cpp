#include <iostream>

#include "BlockStoreClient.cpp"

int main() {
  BlockStoreClient hadev;
  hadev.Write(0, "apple");

  std::cout << hadev.Read(0) << std::endl;
}
