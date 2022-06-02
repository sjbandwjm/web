#pragma once
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <vector>

namespace jiayou::common {

template <typename Duration>
int64_t SteadyTimeGen(int64_t add = 0) {
	auto t = std::chrono::time_point_cast<Duration>(std::chrono::steady_clock::now());
	return t.time_since_epoch().count() + add;
}

class TimerThread {
public:
	using Callbcak = std::function<void(void)>;
	TimerThread();
	~TimerThread();

	struct Task {
		Task(Callbcak&& cb, int64_t abstime_us) : cb(std::move(cb)), run_abstime_us(abstime_us) {}
		Callbcak cb;
		int64_t run_abstime_us;
		std::atomic_bool is_cancle{false};
	};
	using TaskPtr = std::shared_ptr<Task>;
	using TaskWPtr = std::weak_ptr<Task>;

	void Start();
	void Stop();
	TaskWPtr Schedule(Callbcak&& cb, int64_t abstime_ms);
	void UnSchedule(TaskWPtr task);

private:
	void run();
	int64_t now();

	std::thread work_thread_;
	std::atomic_bool is_stop_{false};
	std::atomic_bool is_started_{false};
	std::mutex mu_;
	std::condition_variable cv_;
	std::vector<TaskPtr> tasks_;
	int64_t nearest_run_time_us_;
	int64_t nsignals_{0};
};

TimerThread* GetGlobalTimer();

}
