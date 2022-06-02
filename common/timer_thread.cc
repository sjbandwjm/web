#include "timer_thread.h"
#include "thread_name_builder.h"
#include <assert.h>
#include <algorithm>

namespace jiayou::common {
static inline bool TaskGreater(const TimerThread::TaskPtr& lhs, const TimerThread::TaskPtr& rhs) {
	return lhs->run_abstime_us > rhs->run_abstime_us;
}


inline int64_t TimerThread::now() {
	return SteadyTimeGen<std::chrono::microseconds>();
}

TimerThread::TimerThread() {
	tasks_.reserve(1024);
	nearest_run_time_us_ = std::numeric_limits<int64_t>::max();
}

TimerThread::~TimerThread() {
	// is_stop_.store(true);
	if (is_started_.load(std::memory_order_acquire)) {
			work_thread_.join();
	}
}

void TimerThread::Start() {
	if (is_started_) {
		return;
	}

	work_thread_ = std::thread([this](){
		HOLO_SET_THREAD_NAME_STATIC_FUNC(timer_thread)
		this->run();
	});
	is_started_ = true;
}

void TimerThread::Stop() {
	is_stop_.store(true);
	if (is_started_.load(std::memory_order_acquire)) {
		work_thread_.join();
	}
}

TimerThread::TaskWPtr TimerThread::Schedule(Callbcak&& cb, int64_t abstime_ms) {
	assert(is_started_.load());
	using namespace std::chrono;

	bool earlier = false;
	int64_t runtime_us = duration_cast<microseconds>(milliseconds(abstime_ms)).count();
	auto taskptr = std::make_shared<Task>(std::move(cb), runtime_us);
	{
		std::lock_guard<std::mutex> lk(mu_);
		tasks_.emplace_back(taskptr);
		if (runtime_us < nearest_run_time_us_) {
			nearest_run_time_us_ = runtime_us;
			// need wake up
			earlier = true;
			nsignals_++;
		}
	}

	if (earlier) {
			cv_.notify_one();
	}

	return taskptr;
}

void TimerThread::UnSchedule(TaskWPtr task) {
	if (auto p = task.lock()) {
		p->is_cancle.store(true);
	}
}

void TimerThread::run() {

	std::vector<TaskPtr> run_tasks;
	run_tasks.reserve(4096);

	while (!is_stop_.load(std::memory_order_relaxed)) {
			// clear _nearest_run_time before consuming tasks
			// this help us to know whether have new task to be scheduled
		std::vector<TaskPtr> tmp;
		tmp.reserve(1024);
		{
				std::lock_guard<std::mutex> lk(mu_);
				// std::make_heap(tasks_.begin(), tasks_.end(), TaskGreater);
				tmp.swap(tasks_);
				nearest_run_time_us_ = std::numeric_limits<int64_t>::max();
		}

		while (!tmp.empty()) {
				run_tasks.emplace_back(std::move(tmp.back()));
				std::push_heap(run_tasks.begin(), run_tasks.end(), TaskGreater);
				tmp.pop_back();
		}

		bool pull_again = false;
		while (!run_tasks.empty()) {
				Task* task = run_tasks.front().get();
				if (now() < task->run_abstime_us) { // not ready
						break;
				}

				{
						std::lock_guard<std::mutex> lk(mu_);
						// new task need to run
						if (nearest_run_time_us_ < task->run_abstime_us) {
								pull_again = true;
								break;
						}
				}

				if (!task->is_cancle) {
						task->cb();
				}

				std::pop_heap(run_tasks.begin(), run_tasks.end(), TaskGreater);
				run_tasks.pop_back();
		}

		if (pull_again) {
				continue;
		}

		int64_t next_run_time = std::numeric_limits<int64_t>::max();
		if (!run_tasks.empty()) {
				next_run_time = run_tasks.front()->run_abstime_us;
		}

		int64_t expected_signal = 0;
		{
				std::lock_guard<std::mutex> lk(mu_);
				if (next_run_time > nearest_run_time_us_) {
						// a task is earlier that what we would wait for
						// we need pull again
						continue;
				} else {
						nearest_run_time_us_ = next_run_time;
						expected_signal = nsignals_;
				}
		}

		const int64_t now_us = now();
		int64_t next_timeout_us = 1000000;
		if (next_run_time != std::numeric_limits<int64_t>::max()) {
				next_timeout_us = next_run_time - now_us;
				if (next_timeout_us > 10000000) {
						next_timeout_us = 10000000;
				}
		}
		// wait for
		std::unique_lock<std::mutex> lk(mu_);
		cv_.wait_for(lk, std::chrono::microseconds(next_timeout_us), [=](){
				return nsignals_ != expected_signal;
		});
	}
}

TimerThread* GetGlobalTimer() {
	static TimerThread global_ins;
	static std::once_flag global_init_once;
	std::call_once(global_init_once, [&](){
			global_ins.Start();
	});
	return &global_ins;
}

}
