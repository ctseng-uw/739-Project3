#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <queue>
#include <utility>

class ServerState {
 public:
  struct LogEnt {
    uint64_t seq;
    int64_t addr;
    std::string data;
  };
  explicit ServerState(bool is_primary) {
    this->is_primary.store(is_primary);
    this->is_backup_alive.store(false);
  }

  void WaitUntilIsPrimary() { is_primary.wait(0); }

  void Add(int64_t addr, const std::string &data) {
    mutex.lock();
    auto next_seq = seq.fetch_add(1);
    log.push({next_seq, addr, data});
    mutex.unlock();
  }

  std::pair<int64_t, std::unique_ptr<std::string>> Pop() {
    mutex.lock();
    if (log.empty()) {
      mutex.unlock();
      return {-1, nullptr};
    }
    auto [seq, addr, data] = log.front();
    log.pop();
    std::pair<int64_t, std::unique_ptr<std::string>> result = {
        addr, std::make_unique<std::string>(data)};
    mutex.unlock();
    return result;
  }

  void DeclareBackupDead() {
    if (is_backup_alive.load() == false) return;
    puts("Declare backup dead");
    is_backup_alive.store(false);
  }

  void DeclareBackupNormal() {
    if (is_backup_alive.load() == true) return;
    puts("Declare backup normal");
    is_backup_alive.store(true);
  }

  bool IsBackupAlive() { return is_backup_alive.load(); }

 private:
  std::atomic<bool> is_primary;
  std::atomic<bool> is_backup_alive;
  std::atomic<uint64_t> seq;
  std::queue<LogEnt> log;
  std::mutex mutex;
};
