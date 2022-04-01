#pragma once
#include <chrono>
#include <condition_variable>

using namespace std::chrono_literals;

class TimeoutWatcher {
  std::condition_variable cv;
  std::mutex cv_m;
  bool heartbeat_received;

 public:
  void BlockUntilHeartbeatTimeout() {
    while (true) {
      std::unique_lock<std::mutex> lk(cv_m);
      auto ret =
          cv.wait_for(lk, 100ms, [this] { return heartbeat_received == true; });
      if (ret == true) {
        heartbeat_received = false;
      } else {
        return;
      }
    }
  }

  void NotifyHeartBeat() {
    {
      std::lock_guard<std::mutex> lk(cv_m);
      heartbeat_received = true;
    }
    cv.notify_all();
  }
};