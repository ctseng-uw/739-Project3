#pragma once
#include <grpcpp/grpcpp.h>
#include <grpcpp/support/status_code_enum.h>

#include <exception>
#include <iomanip>
#include <queue>
#include <thread>

#include "includes/heartbeat.grpc.pb.h"
#include "includes/heartbeat.pb.h"
#include "macro.h"
#include "magic.h"

using namespace std::chrono_literals;

const std::array<std::array<std::string, 2>, 2> LAN_ADDR{
    {{{"node0", "node0.hadev3.advosuwmadison-pg0.wisc.cloudlab.us"}},
     {{"node1", "node1.hadev3.advosuwmadison-pg0.wisc.cloudlab.us"}}}};

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
    int64_t addr;
    std::string data;
  };

  std::unique_ptr<hadev::Heartbeat::Stub> stub_;
  std::atomic<bool> is_backup_alive;
  std::queue<LogEnt> log;
  std::mutex mutex;
  std::shared_ptr<bool> i_am_primary;
  int my_node_number, current_iface;

  int BeatHeart() {
    hadev::Request request;
    grpc::ClientContext context;
    hadev::Reply reply;

    context.set_wait_for_ready(true);
    context.set_deadline(std::chrono::system_clock::now() + 2s);

    grpc::Status status = stub_->RepliWrite(&context, request, &reply);
    if (!status.ok()) {
      std::cout << "Fail once" << std::endl;
      SwitchIface(1 - current_iface);
      grpc::ClientContext context;
      context.set_wait_for_ready(true);
      context.set_deadline(std::chrono::system_clock::now() + 2s);
      status = stub_->RepliWrite(&context, request, &reply);
    }
    if (!status.ok()) {
      std::cout << "Fail twice" << std::endl;
      return -1;
    }
    return reply.i_am_primary();
  }

  // Return value: 0=OK 1=other_side_is_primary >=2=timeout
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
      SwitchIface(1 - current_iface);
      grpc::ClientContext context;
      context.set_wait_for_ready(true);
      context.set_deadline(std::chrono::system_clock::now() + 2s);
      hadev::Request request;
      request.set_addr(addr);
      request.set_data(data);
      hadev::Reply reply;
      status = stub_->RepliWrite(&context, request, &reply);
    }
    if (!status.ok()) {
      assert(status.error_code() >= 2);
      return status.error_code();
    }
    return reply.i_am_primary();
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
      } else if (rc >= 2) {
        std::cout << "Remote dead while recovering" << std::endl;
        break;
      }
    }

    std::cout << MAGIC_RECOVERY_COMPLETE << std::endl;
    return cnt;
  }

  void SwitchIface(int iface) {
    std::cout << "Switch to iface" << iface << std::endl;
    current_iface = iface;
    grpc::ChannelArguments ch_args;
    ch_args.SetMaxReceiveMessageSize(INT_MAX);
    ch_args.SetMaxSendMessageSize(INT_MAX);
    ch_args.SetInt(GRPC_ARG_MAX_RECONNECT_BACKOFF_MS, 100);
    auto channel = grpc::CreateCustomChannel(
        LAN_ADDR[1 - my_node_number][current_iface] + ":" + PORT,
        grpc::InsecureChannelCredentials(), ch_args);
    stub_ = hadev::Heartbeat::NewStub(channel);
  }

 public:
  HeartbeatClient(int my_node_number, std::shared_ptr<bool> i_am_primary)
      : my_node_number(my_node_number), i_am_primary(i_am_primary) {
    is_backup_alive.store(0);
    SwitchIface(0);
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
        log.push({addr, data});
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
      std::cout << std::setfill('0') << std::setw(4) << i++
                << " Call BeatHeart " << remote_status << std::endl;
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
