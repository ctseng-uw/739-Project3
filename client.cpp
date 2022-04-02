#include <iostream>

#include "BlockStoreClient.cpp"

int main(int argc, char **argv) {
  BlockStoreClient hadev(argv[1][0] == '0' ? "10.10.1.1:23576"
                                           : "10.10.1.2:23576");
  if (argv[2][0] == 'r') {
    std::cout << hadev.Read(std::atoi(argv[3]));
  } else {
    int rc = hadev.Write(std::atoi(argv[3]), argv[4]);
    switch (rc) {
      case -1:
        puts("Fail to contact server");
        break;
      case 0:
        puts("Successful write");
        break;
      case 1:
        puts("Server is backup. Didn't write");
        break;
    }
  }
}
