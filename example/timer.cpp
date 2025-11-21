#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <memory>
#include <functional>
#include <coroutine>
#include <queue>
#include <ranges>

#include <stdexec/execution.hpp>
#include <nagisa/concurrency/concurrency.h>

#if __has_include(<pthread.h>)
#	define USE_PTHREAD 1
#	include <pthread.h>
#else
#	define USE_PTHREAD 0
#endif

namespace nc = ::nagisa::concurrency;
template<class TimePoint>
struct timer_task_t
{
	using time_point = TimePoint;
	::std::coroutine_handle<> handle;
	time_point due;
	constexpr auto operator<=>(timer_task_t const& other) const noexcept { return due <=> other.due; }
};

template<class Scheduler, class Clock = ::std::chrono::steady_clock>
struct timer
{
	// using Scheduler = ::stdexec::inline_scheduler;
	// using Clock = ::std::chrono::steady_clock;
	using self_type = timer;
	using scheduler_type = Scheduler;
	using clock_type = Clock;
	using time_point_type = typename clock_type::time_point;
	using duration_type = typename clock_type::duration;

	struct task_queue
	{
		using task_type = timer_task_t<time_point_type>;

		void push(task_type const& task) noexcept
		{
			auto lk = ::std::unique_lock(_mutex);
			_tasks.push(task);
			_cv.notify_one();
		}
		::std::optional<task_type> pop(::stdexec::stoppable_token auto const& stop_token) noexcept
		{
			auto lk = ::std::unique_lock(_mutex);
			auto notify = [this] { _cv.notify_all(); };
			auto callback = ::stdexec::stop_callback_for_t<::std::remove_cvref_t<decltype(stop_token)>, decltype(notify)>(stop_token, notify);
			while (true)
			{
				if (_tasks.empty())
					_cv.wait(lk, [this, &stop_token] { return stop_token.stop_requested() || !_tasks.empty(); });
				if (stop_token.stop_requested())
					return ::std::nullopt;
				auto task = _tasks.top();
				if (task.due <= clock_type::now())
				{
					_tasks.pop();
					return task;
				}
				auto status = _cv.wait_until(lk, task.due);
				if (stop_token.stop_requested())
					return ::std::nullopt;
			}
		}

		::std::mutex _mutex{};
		::std::condition_variable _cv{};
		::std::priority_queue<task_type, ::std::vector<task_type>, ::std::greater<>> _tasks{};
	};

	auto run(::stdexec::stoppable_token auto const& stop_token) noexcept
	{
		while (!stop_token.stop_requested())
		{
			auto task = _queue.pop(stop_token);
			if (!task)
				return;
			::nagisa::concurrency::spawn(_other_scheduler, ::nagisa::concurrency::sync_invoke{ [task]
				{
					task->handle.resume();
				} });
		}
	}

	struct scheduler
	{
		self_type* _self;
		time_point_type _time_point = {};
		struct at : ::std::suspend_always
		{
			constexpr auto&& get_env() const noexcept { return *this; }
			template<class Tag>
			constexpr auto query(::stdexec::get_completion_scheduler_t<Tag>) const noexcept { return scheduler{ _self }; }
			auto await_suspend(::std::coroutine_handle<> parent) const noexcept
			{
				return _self->_queue.push({ parent, _due });
			}
			self_type* _self;
			time_point_type _due;
		};
		constexpr static auto now() noexcept { return clock_type::now(); }
		constexpr auto schedule() const noexcept { return scheduler::schedule_at(_time_point); }
		constexpr auto schedule_at(time_point_type tp) const noexcept { return at{ {}, _self, tp }; }
		constexpr bool operator==(const scheduler&) const = default;
	};
	constexpr auto get_scheduler() noexcept { return scheduler{ this }; }
	constexpr auto get_scheduler(time_point_type time_point) noexcept { return scheduler{ this, time_point }; }

	timer(scheduler_type scheduler) : _other_scheduler(scheduler) {}

	task_queue _queue{};
	scheduler_type _other_scheduler;
};
template<class Scheduler>
timer(Scheduler) -> timer<Scheduler>;

using clock_type = ::std::chrono::system_clock;
clock_type::time_point current_now = clock_type::now();

::std::mutex mutex;
::nc::simple_task<void> print(::stdexec::inplace_stop_token stop_token, int id)
{
	using ::std::chrono::duration_cast;
	using ::std::chrono::milliseconds;

	::std::unique_lock l(mutex);
	::std::cout << duration_cast<milliseconds>(clock_type::now() - current_now).count()
				<< " ms:  [coro] id=" << id
				<<  '\n';
	co_return;
}

int main() {
	using namespace ::std::chrono_literals;

	::stdexec::run_loop loop{};
	auto t = timer<decltype(loop.get_scheduler()), clock_type>(loop.get_scheduler());
	auto stop_source = ::stdexec::inplace_stop_source{};
	auto __ = ::std::jthread([&] { loop.run();  });
#if USE_PTHREAD
	{
		auto handle = __.native_handle();
		sched_param sch_params{};
		sch_params.sched_priority = 90;
		if (pthread_setschedparam(handle, SCHED_FIFO, &sch_params) != 0) {
			std::cerr << "Failed to set SCHED_FIFO, need sudo or CAP_SYS_NICE\n";
		}
		else {
			std::cout << "Thread switched to SCHED_FIFO priority "
				<< priority << "\n";
		}
	}
#endif
	auto _ = ::std::jthread([&] { t.run(stop_source.get_token());  });

	current_now = clock_type::now();
	for (auto [id, duration] : ::std::array{
			3000ms,
			100ms,
			150ms,
			500ms,
			300ms,
			2000ms,
		} | ::std::views::enumerate)
	{
		::nc::spawn(t.get_scheduler(current_now + duration), ::print(stop_source.get_token(), id));
	}
	::std::this_thread::sleep_for(5s);
	std::cout << "main: requesting stop\n";
	stop_source.request_stop();
	loop.finish();
	return 0;
}