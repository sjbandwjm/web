#pragma once
#include <condition_variable>
#include <atomic>
#include <memory>
#include "butil/time.h"
#include "bthread/mutex.h"
#include "bthread/bthread.h"
#include "bthread/condition_variable.h"

namespace jiayou {
namespace common {

// note: 当此类析构时不能再被使用
class WaitGroup {
 public:
  WaitGroup() {}

  int Add(int runing_thread_cnt) {
    return runing_thread_cnt_.fetch_add(runing_thread_cnt);
  }

  int Done() {
    auto ret = runing_thread_cnt_.fetch_sub(1, std::memory_order_relaxed);
    if ( ret == 1 ) {
      cv_.notify_one();
    }
    return ret;
  }

  //return: runing thread count
  int BusyWait(int64_t timeout_us = -1) {
    auto start = butil::cpuwide_time_us();
    while (auto cnt = runing_thread_cnt_.load() != 0) {
      if (timeout_us >= 0 && butil::cpuwide_time_us() - start >= timeout_us) {
        return cnt;
      }
    }
    return 0;
  }

  // 注意这里返回值可能为负值
  inline int GetRuningCount() {
    return runing_thread_cnt_.load();
  }

  int BlockWait(int timeout_us = -1) {
    std::unique_lock<bthread::Mutex> lk(mu_);
    auto condition = [this](){ return runing_thread_cnt_.load() <= 0; };
    if (timeout_us >= 0) {
      //cv_.wait_for(lk, std::chrono::microseconds(timeout_us), std::move(condition));
      wait_for_(lk, timeout_us, std::move(condition));
    } else {
      //cv_.wait(lk, std::move(condition));
      wait_(lk, std::move(condition));
    }
    return GetRuningCount();
  }

 private:
  template<typename Predicate>
  int wait_for_(std::unique_lock<bthread::Mutex>& lk, long timeout_us, Predicate __p) {
    int ret = 0;
    while(!__p()) {
      ret = cv_.wait_for(lk, timeout_us);
      if (ret != 0) {
        break;
      }
    }

    return ret;
  }

  template<typename Predicate>
  void wait_(std::unique_lock<bthread::Mutex>& lk, Predicate __p) {
    while(!__p()) {
      cv_.wait(lk);
    }
  }

 private:
  WaitGroup(const WaitGroup&) = delete;
  WaitGroup(WaitGroup&&) = delete;
  WaitGroup& operator=(const WaitGroup&) = delete;
  WaitGroup& operator=(WaitGroup&&) = delete;

  std::atomic<int> runing_thread_cnt_{0};
  bthread::Mutex mu_;
  bthread::ConditionVariable cv_;
  //std::condition_variable cv_;
};

// 某些情况下，当某个WaitGroup已经deconstruction时，无法保证不再继续使用该对象，推荐使用WaitGroupPtr
typedef std::shared_ptr<WaitGroup> WaitGroupPtr;

}
}