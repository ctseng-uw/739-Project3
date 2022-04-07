#pragma once
#include <grpcpp/support/status_code_enum.h>

#include <exception>
#include <iomanip>
#include <queue>
#include <thread>

#include "includes/heartbeat.grpc.pb.h"
#include "includes/heartbeat.pb.h"
#include "magic.h"
// const uint32_t TIMEOUTMS = 10;
using namespace std::chrono_literals;

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
    std::cout << "Call Write" << std::endl;

    hadev::Request request;
    grpc::ClientContext context;
    hadev::Reply reply;
    context.set_wait_for_ready(true);
    context.set_deadline(std::chrono::system_clock::now() + 2s);
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
    std::cout << MAGIC_STARTING_RECOVERY << std::endl;
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
        std::lock_guard<std::mutex> lg(mutex);
        log.pop();
      } else if (rc == 1) {
        std::cout << "Both side have logs and think thry're primary!"
                  << std::endl;
        assert(false);
      } else if (rc == 2) {
        std::cout << "Remote dead while recovering" << std::endl;
        break;
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
    std::cout << "PRIMARY calls Write. Write to backup or log?" << std::endl;
    int rc = 99999;  // 0=OK 1=other_side_is_primary 2=timeout
    if (is_backup_alive.load()) {
      std::cout << "Write to backup" << std::endl;
      rc = RepliWrite(addr, data);
    }

    if (rc == 0) {
      std::cout << "Successfully wrote to backup" << std::endl;
    } else if (rc == 1) {
      std::cout << "Failed to write backup" << std::endl;
      return false;
    } else if (rc >= 2) {
      std::cout << "Status code=" << rc << ". Write to log\n";
      {
        std::lock_guard<std::mutex> lock(mutex);
        log.push({seq.fetch_add(1), addr, data});
        if (is_backup_alive.load() == true) {
          std::cout << "Backup dead (write)" << std::endl;
        }
        is_backup_alive.store(0);
      }
    }
    return true;
  }

  void BlockUntilBecomeBackup() {
    using namespace std::chrono_literals;
    int i = 0;  // just to see if server is running...
    std::cout << "BlockUntilBecomeBackup" << std::endl;
    while (true) {
      if (*i_am_primary == false) {
        return;
      }
      int remote_status = BeatHeart();
      std::cout << std::setfill('0') << std::setw(4) << i++ << " Call BeatHeart"
                << std::endl;
      if (remote_status >= 0) {
        if (is_backup_alive.load() == false) {
          std::cout << "Remote alive (heartbeat)." << std::endl;
          int recovery_cnt = RunRecovery();
          if (is_backup_alive.load() == true && remote_status == 1 &&
              recovery_cnt == 0) {
            std::cout << "Remote think it is Primary and I had no log"
                      << std::endl;
            std::cout << "PRIMARY to BACKUP" << std::endl;
            *i_am_primary = false;
            return;
          }
        } else {  // is_backup_alive.load() == true
          if (remote_status == 1) {
            std::cout << "Remote think it is Primary and I had no log"
                      << std::endl;
            std::cout << "PRIMARY to BACKUP" << std::endl;
            *i_am_primary = false;
          }
        }
      } else {
        if (is_backup_alive.load() == true) {
          std::cout << "Remote dead (heartbeat)" << std::endl;
          is_backup_alive.store(0);
        }
      }
      std::this_thread::sleep_for(200ms);
    }
  }
};
