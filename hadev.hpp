#pragma once
#include <memory>
#include <string>

class HadevClient {
 public:
  HadevClient();
  ~HadevClient();
  int Write(const int64_t, const std::string &);
  std::string Read(const int64_t);

 private:
  class BlockStoreClient;
  std::unique_ptr<BlockStoreClient> blockstoreClient;
};
