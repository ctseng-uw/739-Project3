#pragma once
#include <time.h>

#include <exception>
#include <queue>

#include "includes/heartbeat.grpc.pb.h"
#include "includes/heartbeat.pb.h"
#include "magic.h"
#include "server.h"
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
  std::shared_ptr<ServerState> server_state;
  std::shared_ptr<struct timespec> last_heartbeat;
  const int node_num;

  auto BeatHeart() {
    hadev::Request request;
    grpc::ClientContext context;
    hadev::Reply reply;

    grpc::Status status = stub_->RepliWrite(&context, request, &reply);
    return status;
  }

  ServerState GetState() {
    hadev::State request;
    grpc::ClientContext context;
    hadev::State reply;
    request.set_state(static_cast<int32_t>(*server_state));
    auto status = stub_->GetState(&context, request, &reply);
    if (!status.ok()) {
      throw RPCFailException(status);
      // throw std::exception();
    }
    return static_cast<ServerState>(reply.state());
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

    std::cout << MAGIC_RECOVERY_COMPLETE << std::endl;
  }

  void InitStateChange() {
    while (*server_state == INIT) {
      ServerState remote_state;
      try {
        remote_state = GetState();
      } catch (RPCFailException e) {
        std::cerr
            << "InitStateChange: Fail to connect to another node. Retry\n";
        continue;
      }

      if (*server_state != INIT) {
        break;
      }

      if (remote_state == INIT) { // Should never occur
        throw std::runtime_error("InitStateChange: unexpected GetState() result");
        // if (node_num == 0) {
        //   *server_state = PRIMARY;
        // } else {
        //   *server_state = BACKUP;
        // }
      } else if (remote_state == BACKUP) {
        *server_state = PRIMARY;
      } else if (remote_state == PRIMARY) {
        *server_state = BACKUP;
      } else {
        throw std::runtime_error("InitStateChange: bad GetState() result");
      }
      break;
    }
    assert(*server_state != INIT);
    std::cout << "InitStateChange: state changed to "
              << (*server_state == PRIMARY ? "PRIMARY" : "BACKUP") << "\n";
  }

 public:
  HeartbeatClient(const std::shared_ptr<grpc::ChannelInterface>& channel,
                  std::shared_ptr<ServerState> server_state,
                  std::shared_ptr<struct timespec> last_heartbeat,
                  int node_num)
      : stub_(hadev::Heartbeat::NewStub(channel)),
        server_state(server_state),
        last_heartbeat(last_heartbeat),
        node_num(node_num) {
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
    // Solve INIT
    InitStateChange();

    // Condition variable: send heartbeat only when state==PRIMARY
    clock_gettime(CLOCK_MONOTONIC, &*last_heartbeat);
    while (true) {
      // TODO: change
      if (*server_state == BACKUP) {
        usleep(TIMEOUTMS * 300);
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        float diff_ms =
            (now.tv_sec - (*last_heartbeat).tv_sec) * 1000 +
            (float)(now.tv_nsec - (*last_heartbeat).tv_nsec) / 1000000;
        if (diff_ms > TIMEOUTMS) {
          *server_state = PRIMARY;
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

  void TimeoutCheck() {}
};