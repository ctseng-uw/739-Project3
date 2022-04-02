#pragma once
#include <chrono>
#include <condition_variable>

using namespace std::chrono_literals;

class TimeoutWatcher {
  std::condition_variable cv;
  std::mutex cv_m;
  bool heartbeat_received;

 public:
  void BlockUntilRcvFirstHeartbeat() {
    std::unique_lock<std::mutex> lk(cv_m);
    cv.wait(lk);
  }

  void BlockUntilHeartbeatTimeout() {
    puts("Enter BlockUntilHeartbeatTimeout");
    int i = 0;  // Debug
    while (true) {
      std::unique_lock<std::mutex> lk(cv_m);
      auto ret =
          cv.wait_for(lk, 1s, [this] { return heartbeat_received == true; });
      if (ret == true) {
        printf("%4d HB Rcvr: Reset timer\n", i++);
        heartbeat_received = false;
      } else {
        printf("%4d HB Rcvr: Timeout\n", i++);
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
