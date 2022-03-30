#pragma once
#include <exception>
#include <queue>

#include <time.h>

#include "includes/heartbeat.grpc.pb.h"
#include "includes/heartbeat.pb.h"
const uint32_t TIMEOUTMS = 10;

class RPCFailException : public std::runtime_error {
 public:
  RPCFailException(grpc::Status status)
      : std::runtime_error("gRPC status " +
                           std::to_string(status.error_code()) + ": " +
                           status.error_message()) {}
};

class HeartbeatClient {
 private:
  struct LogEnt {
    uint64_t seq;
    int64_t addr;
    std::string data;
  };

  std::unique_ptr<hadev::Heartbeat::Stub> stub_;
  std::atomic<bool> is_backup_alive;
  std::atomic<uint64_t> seq;
  std::queue<LogEnt> log;
  std::mutex mutex;
  std::shared_ptr<int> i_am_primary;
  std::shared_ptr<struct timespec> last_heartbeat;
  int my_node_number;

  auto BeatHeart() {
    hadev::Request request;
    grpc::ClientContext context;
    hadev::Reply reply;

    grpc::Status status = stub_->RepliWrite(&context, request, &reply);
    return status;
  }

  int is_primary() {
    hadev::Blank request;
    grpc::ClientContext context;
    hadev::ReplyState reply;
    auto status = stub_->is_primary(&context, request, &reply);
    if (!status.ok()) {
      throw RPCFailException(status);
      // throw std::exception();
    }
    return reply.state();
  }

  grpc::Status RepliWrite(int64_t addr, const std::string& data) {
    hadev::Request request;
    grpc::ClientContext context;
    hadev::Reply reply;
    request.set_addr(addr);
    request.set_data(data);
    grpc::Status status = stub_->RepliWrite(&context, request, &reply);
    return status;
  };

  void RunRecovery() {
    puts("Starting recovery");
    LogEnt ent;
    while (true) {
      {
        std::lock_guard<std::mutex> lg(mutex);
        if (log.empty()) {
          is_backup_alive.store(1);
          break;
        }
        ent = log.front();
      }
      auto status = RepliWrite(ent.addr, ent.data);
      if (!status.ok()) {
        puts("Backup dead while recovering");
        break;
      }
      {
        std::lock_guard<std::mutex> lg(mutex);
        log.pop();
      }
    }
    puts("Recovery completed");
  }

  void setIAMPrimary() {
    while (true) {
      int you_are_primary;
      try {
        you_are_primary = is_primary();
      }
      catch (RPCFailException e) {
        continue;
      }
      if (you_are_primary == 0) {
        if (my_node_number == 0) {
          *i_am_primary = 2;
        }
        else {
          *i_am_primary = 1;
        }
      }
      else if (you_are_primary == 1) {
        *i_am_primary = 2;
      }
      else if (you_are_primary == 2) {
        *i_am_primary = 1;
      }
      break;
    }
  }

 public:
  HeartbeatClient(const std::shared_ptr<grpc::ChannelInterface>& channel,
                  std::shared_ptr<int> i_am_primary,
                  std::shared_ptr<struct timespec> last_heartbeat,
                  int my_node_number)
      : stub_(hadev::Heartbeat::NewStub(channel)), i_am_primary(i_am_primary),
        last_heartbeat(last_heartbeat), my_node_number(my_node_number) {
    is_backup_alive.store(0);
    seq.store(0);
  };

  void Write(int64_t addr, const std::string& data) {
    bool ok = false;
    if (is_backup_alive.load()) {
      puts("Write to backup");
      auto status = RepliWrite(addr, data);
      if (status.ok()) ok = true;
    }
    if (!ok) {
      puts("Write to log");
      {
        std::lock_guard<std::mutex> lock(mutex);
        log.push({seq.fetch_add(1), addr, data});
        if (is_backup_alive.load() == true) {
          puts("Backup dead (write)");
        }
        is_backup_alive.store(0);
      }
    }
  }

  void Start() {
    setIAMPrimary();
    clock_gettime(CLOCK_MONOTONIC, &*last_heartbeat);
    while (true) {
      // TODO:
      if (*i_am_primary == 1) {
        usleep(TIMEOUTMS * 300);
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        float diff_ms = (now.tv_sec - (*last_heartbeat).tv_sec) * 1000 +
                    (float)(now.tv_nsec - (*last_heartbeat).tv_nsec) / 1000000;
        if (diff_ms > TIMEOUTMS) {
          *i_am_primary = 2;
        }
        continue;
      }
      auto status = BeatHeart();
      if (status.ok()) {
        if (is_backup_alive.load() == false) {
          RunRecovery();
        }
        usleep(TIMEOUTMS * 300);
      } else {
        if (is_backup_alive.load() == true) {
          puts("Backup dead (heartbeat)");
          is_backup_alive.store(0);
        }
      }
    }
  }
};