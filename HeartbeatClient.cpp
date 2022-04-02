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

  grpc::Status BeatHeart() {
    hadev::Request request;
    grpc::ClientContext context;
    hadev::Reply reply;

    auto deadline =
        std::chrono::system_clock::now() + std::chrono::milliseconds(100);
    context.set_deadline(deadline);

    grpc::Status status = stub_->RepliWrite(&context, request, &reply);
    return status;
  }

  // Return value: 0=OK 1=other_side_is_primary 2=timeout
  int RepliWrite(int64_t addr, const std::string& data) {
    puts("Call Write");

    hadev::Request request;
    grpc::ClientContext context;
    hadev::Reply reply;
    request.set_addr(addr);
    request.set_data(data);
    grpc::Status status = stub_->RepliWrite(&context, request, &reply);
    if (!status.ok())
      return 2;
    else if (reply.i_am_primary())
      return 1;
    return 0;
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
      int rc = RepliWrite(ent.addr, ent.data);
      assert(rc != 1);  // It cannot think it is primary!
      if (rc == 2) {
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

  // Return: True==Write success (to remote or log).
  //         False==Remote is primary. Did not write.
  bool Write(int64_t addr, const std::string& data) {
    puts("PRIMARY calls Write. Write to backup or log?");
    int rc = 2;  // 0=OK 1=other_side_is_primary 2=timeout
    if (is_backup_alive.load()) {
      puts("Write to backup");
      rc = RepliWrite(addr, data);
    }

    if (rc == 0) {
      puts("Successfully wrote to backup");
    } else if (rc == 1) {
      puts("Failed to write backup");
      return false;
    } else if (rc == 2) {
      puts("Backup timeout. Write to log");
      {
        std::lock_guard<std::mutex> lock(mutex);
        log.push({seq.fetch_add(1), addr, data});
        if (is_backup_alive.load() == true) {
          puts("Backup dead (write)");
        }
        is_backup_alive.store(0);
      }
    }
    return true;
  }

  void BlockUntilBecomeBackup() {
    int i = 0;  // just to see if server is running...
    puts("BlockUntilBecomeBackup");
    while (true) {
      if (*i_am_primary == false) {
        return;
      }
      auto remote_status = BeatHeart();
      printf("%4d Call BeatHeart\n", i++);
      if (remote_status.ok()) {
        if (is_backup_alive.load() == false) {
          RunRecovery();
        }
      } else {
        if (is_backup_alive.load() == true) {
          puts("Backup dead (heartbeat)");
          is_backup_alive.store(0);
        } else {
          puts("Backup is still dead");
        }
      }
      usleep(200000);  // send heartbeat every 200 ms
    }
  }
};
