#pragma once
#include <grpcpp/support/status_code_enum.h>

#include <exception>
#include <queue>

#include "includes/heartbeat.grpc.pb.h"
#include "includes/heartbeat.pb.h"
#include "magic.h"
// const uint32_t TIMEOUTMS = 10;

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
  std::shared_ptr<bool> i_am_primary;

  auto BeatHeart() {
    puts("Call BeatHeart");

    hadev::Request request;
    grpc::ClientContext context;
    hadev::Reply reply;

    auto deadline =
        std::chrono::system_clock::now() + std::chrono::milliseconds(100);
    context.set_deadline(deadline);

    grpc::Status status = stub_->RepliWrite(&context, request, &reply);
    // puts("BeatHeart");
    return status;
  }

  grpc::Status RepliWrite(int64_t addr, const std::string& data) {
    puts("Call Write");

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

    std::cout << MAGIC_RECOVERY_COMPLETE << std::endl;
  }

 public:
  HeartbeatClient(const std::shared_ptr<grpc::ChannelInterface>& channel,
                  std::shared_ptr<bool> i_am_primary)
      : stub_(hadev::Heartbeat::NewStub(channel)), i_am_primary(i_am_primary) {
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

  void LoopUntilConflict() {
    puts("LoopUntilConflict");
    while (true) {
      auto status = BeatHeart();
      if (status.ok()) {
        if (is_backup_alive.load() == false) {
          RunRecovery();
        }
        // } else if (status.error_code() == grpc::StatusCode::OUT_OF_RANGE) {
        //   puts("Two primaries");
        //   return;
      } else {
        if (is_backup_alive.load() == true) {
          puts("Backup dead (heartbeat)");
          is_backup_alive.store(0);
        }
      }
      sleep(1);
    }
  }
};
