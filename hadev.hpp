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
  class GRPCClient;
  std::unique_ptr<GRPCClient> gRPCClient;
};
