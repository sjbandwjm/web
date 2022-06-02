#pragma once

#include <atomic>
#include <functional>
#include <condition_variable>
#include <map>
#include <memory>
#include <queue>
#include <thread>
#include <bthread/bthread.h>

#include <gflags/gflags.h>

// #include "absl/strings/str_format.h"
#include "butil/object_pool.h"
#include "thunk_entry.h"

namespace jiayou {
namespace base {

template <class T>
class SPSCFifoQueue {
 public:
  struct Node {
    std::atomic<Node*> next;
    T value;
  };

  SPSCFifoQueue() {
    head_ = tail_ = Get();
  }

  ~SPSCFifoQueue() {
    for (auto* node = tail_; node != nullptr; node = node->next) {
      node->value.Reset();
      butil::return_object(node);
    }
  }

  Node* Get() {
    auto* node = butil::get_object<Node>();
    node->next.store(nullptr, std::memory_order_relaxed);
    return node;
  }

  void Push(T&& value) {
    auto* n = Get();
    n->value = std::move(value);
    Push(n);
  }

  void Push(Node* node) {
    head_->next.store(node, std::memory_order_release);
    head_ = node;
    if (tail_ == nullptr) {
      tail_ = head_;
    }
  }

  Node* Pop() {
    auto* node = tail_->next.load(std::memory_order_acquire);
    if (node == nullptr) {
      return nullptr;
    }
    tail_->value.Reset();
    butil::return_object(tail_);
    tail_ = node;
    return node;
  }

 private:
  Node* head_;
  Node* tail_;
};

class SimplePthreadPool {
 public:
  SimplePthreadPool(int num_thread) {
    for (int i = 0; i < num_thread; ++i) {
      threads_.emplace_back(std::thread([this](){
        while (true) {
          std::unique_lock lock(mu_);
          cv_.wait(lock, [this]{ return !tasks_.empty(); });
          auto task = tasks_.front();
          tasks_.pop();
          lock.unlock();
          task.fn(task.arg);
        }
      }));
    }
  }

  void Post(void*(*fn)(void*), void* arg) {
    std::unique_lock lock(mu_);
    tasks_.push(Task{arg, fn});
    lock.unlock();
    cv_.notify_one();
  }

 private:
  struct Task {
    void* arg;
    void*(*fn)(void*);
  };
  std::queue<Task> tasks_;
  std::mutex mu_;
  std::condition_variable cv_;
  std::vector<std::thread> threads_;
};

DECLARE_int32(keyed_spsc_worker_pthread_cnt);
class SPSCExecutor {
 public:
  typedef void (*ExecutorType)(void*(*)(void*), void*);

  static void PthreadExecutor(void*(*fn)(void*), void* arg) {
    static SimplePthreadPool pp(FLAGS_keyed_spsc_worker_pthread_cnt);
    pp.Post(fn, arg);
  }

  static void BthreadExecutor(void*(*fn)(void*), void* arg) {
    bthread_t bid;
    bthread_start_background(&bid, nullptr, fn, arg);
  }

  SPSCExecutor() {
    running_.store(0, std::memory_order_relaxed);
    stopped_.store(0, std::memory_order_relaxed);
  }

  void SetExecutor(ExecutorType executor) {
    executor_ = executor;
  }

  void Init(void(*fn)(void*), void* arg) {
    fn_ = fn;
    arg_ = arg;
  }

  void Notify() {
    int rc = running_.fetch_add(1, std::memory_order_acq_rel);
    if (rc == 0) {
      executor_(ExecuteEntry, this);
    }
  }

  void Start(std::function<void()>&& fn) {
    auto* entry = new ThunkEntry([=]() {
      fn();
      if (stopped_.load(std::memory_order_acquire)) {
        auto detached_deleter = std::move(deleter_);
        detached_deleter();
      } else {
        Execute();
      }
    });
    int rc = running_.fetch_add(1, std::memory_order_acq_rel);
    if (rc != 0) {
      abort();
    }
    executor_(entry->entry(), entry);
  }

  void Join(std::function<void()> deleter) {
    deleter_ = deleter;
    stopped_.store(1, std::memory_order_release);
    int rc = running_.fetch_add(1, std::memory_order_acq_rel);
    if (rc == 0) {
      // no task is running
      auto detached_deleter = std::move(deleter_);
      detached_deleter();
      return;
    } else {
      // task is running or about-to-quit,
      // and should see the stopped flag and remove flag
    }
  }

  void Reset() {
    running_.store(0, std::memory_order_release);
    stopped_.store(0, std::memory_order_release);
    deleter_ = nullptr;
  }

  static void* ExecuteEntry(void* data) {
    static_cast<SPSCExecutor*>(data)->Execute();
    return nullptr;
  }

 private:
  void Execute() {
    int count = 0;
    do {
      count = running_.load(std::memory_order_acquire);
      fn_(arg_);

      if (stopped_.load(std::memory_order_acquire)) {
        auto detached_deleter = std::move(deleter_);
        detached_deleter();
        return;
      }
    } while (MoreTask(count));
  }

  bool MoreTask(int count) {
    int rc = running_.fetch_sub(count, std::memory_order_release);
    if (rc - count == 0) {
      return false;
    }
    return true;
  }

  ExecutorType executor_ = &BthreadExecutor;
  void (*fn_)(void*) = nullptr;
  void* arg_ = nullptr;
  std::atomic<int> running_;
  std::atomic<int> stopped_;
  std::function<void()> deleter_ = nullptr;
};

DECLARE_bool(keyed_spsc_worker_pthread);

template <class Key, class T, class LocalStorage>
class KeyedSPSCWorker {
 public:
  static constexpr int kUnitShardCount = 32;

  void Post(const Key& key, T&& value) {
    size_t hash_value = hasher_(key);
    int shard_index = hash_value % kUnitShardCount;

    std::unique_lock<std::mutex> lock(shards_[shard_index].mu);
    auto& unit = shards_[shard_index].units[key];
    if (!unit) {
      unit = CreateUnit_(LocalStorage{});
    }
    lock.unlock();

    unit->queue.Push(std::move(value));
    unit->executor.Notify();
  }

  void SetLocalStorage(const Key& key, LocalStorage&& value) {
    size_t hash_value = hasher_(key);
    int shard_index = hash_value % kUnitShardCount;

    std::unique_lock<std::mutex> lock(shards_[shard_index].mu);
    auto& unit = shards_[shard_index].units[key];
    if (!unit) {
      unit = CreateUnit_(std::move(value));
    } else {
      unit->local_storage = std::move(value);
    }
  }

  template <class Fn>
  void Stop(const Key& key, Fn&& fn) {
    size_t hash_value = hasher_(key);
    int shard_index = hash_value % kUnitShardCount;

    std::unique_lock<std::mutex> lock(shards_[shard_index].mu);
    auto& units = shards_[shard_index].units;
    auto it = units.find(key);
    if (it == units.end()) {
      return;
    }
    auto* unit = it->second.release();
    units.erase(it);
    lock.unlock();

    unit->executor.Join([unit, fn1=std::move(fn)] {
      auto storage = std::move(unit->local_storage);
      auto* entry = new ThunkEntry([storage, fn2=std::move(fn1)]() { fn2(storage); });
      bthread_t bid;
      bthread_start_background(&bid, nullptr, entry->entry(), entry);
      delete unit;
    });
  }

  void Init(void(*user_fn)(T&&, LocalStorage&)) {
    user_fn_ = user_fn;
  }

 private:
  struct WorkUnit {
    SPSCFifoQueue<T> queue;
    SPSCExecutor executor;
    LocalStorage local_storage;
    void(*user_fn)(T&&, LocalStorage&);
  };


  static void Run(void* arg) {
    auto* unit = static_cast<WorkUnit*>(arg);
    while (true) {
      auto* node = unit->queue.Pop();
      if (node == nullptr) {
        break;
      }
      unit->user_fn(std::move(node->value), unit->local_storage);
    }
  }

  std::unique_ptr<WorkUnit> CreateUnit_(LocalStorage &&value) {
    auto unit = std::make_unique<WorkUnit>();
    unit->executor.Init(Run, unit.get());
    if (FLAGS_keyed_spsc_worker_pthread) {
      unit->executor.SetExecutor(&SPSCExecutor::PthreadExecutor);
    }
    unit->local_storage = std::move(value);
    unit->user_fn = user_fn_;
    return unit;
  }

  void(*user_fn_)(T&&, LocalStorage&);

  struct UnitShard {
    std::mutex mu;
    std::map<Key, std::unique_ptr<WorkUnit>> units;
  };

  std::hash<Key> hasher_;
  UnitShard shards_[kUnitShardCount];
};

}
}
