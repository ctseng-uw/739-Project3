#pragma once
#include <grpcpp/support/status_code_enum.h>

#include <exception>
#include <queue>
#include <thread>

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

  int BeatHeart() {
    hadev::Request request;
    grpc::ClientContext context;
    hadev::Reply reply;

    context.set_wait_for_ready(true);
    grpc::Status status = stub_->RepliWrite(&context, request, &reply);
    if (!status.ok()) {
      return -1;
    }
    return reply.i_am_primary();
  }

  // Return value: 0=OK 1=other_side_is_primary 2=timeout
  int RepliWrite(int64_t addr, const std::string& data) {
    puts("Call Write");

    hadev::Request request;
    grpc::ClientContext context;
    hadev::Reply reply;
    context.set_wait_for_ready(true);
    request.set_addr(addr);
    request.set_data(data);
    grpc::Status status = stub_->RepliWrite(&context, request, &reply);
    if (!status.ok()) {
      assert(status.error_code() >= 2);
      return status.error_code();
    }

    else if (reply.i_am_primary())
      return 1;
    return 0;
  };

  int RunRecovery() {
    puts("Starting recovery");
    int cnt = 0;
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
      if (rc == 0) {
        ++cnt;
      } else if (rc == 1) {
        puts("Both side have logs and think thry're primary!");
        assert(false);
      } else if (rc == 2) {
        puts("Remote dead while recovering");
        break;
      }
      {
        std::lock_guard<std::mutex> lg(mutex);
        log.pop();
      }
    }

    std::cout << MAGIC_RECOVERY_COMPLETE << std::endl;
    return cnt;
  }

 public:
  HeartbeatClient(const std::shared_ptr<grpc::ChannelInterface>& channel,
                  std::shared_ptr<bool> i_am_primary)
      : stub_(hadev::Heartbeat::NewStub(channel)), i_am_primary(i_am_primary) {
    is_backup_alive.store(0);
    seq.store(0);
  };

  bool LogEmpty() { return log.empty(); }

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
    } else if (rc >= 2) {
      std::cout << "gRPC status code=" << rc << ". Write to log\n";
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
    using namespace std::chrono_literals;
    int i = 0;  // just to see if server is running...
    puts("BlockUntilBecomeBackup");
    while (true) {
      if (*i_am_primary == false) {
        return;
      }
      int remote_status = BeatHeart();
      printf("%4d Call BeatHeart\n", i++);
      if (remote_status >= 0) {
        if (is_backup_alive.load() == false) {
          puts("Remote alive (heartbeat).");
          int recovery_cnt = RunRecovery();
          if (is_backup_alive.load() == true && remote_status == 1 &&
              recovery_cnt == 0) {
            puts("Remote think it is Primary and I had no log");
            puts("PRIMARY to BACKUP");
            *i_am_primary = false;
            return;
          }
        }
      } else {
        if (is_backup_alive.load() == true) {
          puts("Remote dead (heartbeat)");
          is_backup_alive.store(0);
        }
      }
      std::this_thread::sleep_for(200ms);
    }
  }
};
