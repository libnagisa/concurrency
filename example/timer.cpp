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
#include <exec/static_thread_pool.hpp>
#include <exec/task.hpp>
#include <nagisa/concurrency/concurrency.h>
#include <exec/timed_scheduler.hpp>


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
		constexpr auto schedule() const noexcept { return schedule_at(now()); }
		constexpr auto schedule_at(time_point_type tp) const noexcept { return at{ {}, _self, tp }; }
		constexpr bool operator==(const scheduler&) const = default;
	};
	constexpr auto get_scheduler() noexcept { return scheduler{ this }; }

	timer(scheduler_type scheduler) : _other_scheduler(scheduler) {}

	task_queue _queue{};
	scheduler_type _other_scheduler;
};
template<class Scheduler>
timer(Scheduler) -> timer<Scheduler>;

using clock_type = ::std::chrono::system_clock;
clock_type::time_point current_now = clock_type::now();

::std::mutex mutex;
::nc::simple_task<void> delay_print(auto sche, ::stdexec::inplace_stop_token stop_token, clock_type::duration duration, int id)
{
	using ::std::chrono::duration_cast;
	using ::std::chrono::milliseconds;

	co_await ::exec::schedule_after(sche, duration);
	::std::unique_lock l(mutex);
	::std::cout << duration_cast<milliseconds>(clock_type::now() - current_now).count()
				<< " ms:  [coro] id=" << id
				<< " on thread " << std::this_thread::get_id()
				<< " duration = " << duration_cast<milliseconds>(duration).count() << " ms"
				<<  '\n';
}

int main() {
	using namespace ::std::chrono_literals;

	auto num_workers = ::std::max(1u, std::thread::hardware_concurrency());
	std::cout << "starting static_thread_pool with " << num_workers << " workers\n";

	exec::static_thread_pool pool{ num_workers };
	auto t = timer<decltype(pool.get_scheduler()), clock_type>(pool.get_scheduler());
	auto stop_source = ::stdexec::inplace_stop_source{};
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
		::nc::spawn(::stdexec::inline_scheduler{}, ::delay_print(t.get_scheduler(), stop_source.get_token(), duration, id));
	}
	::std::this_thread::sleep_for(5s);
	std::cout << "main: requesting stop\n";
	stop_source.request_stop();
	pool.request_stop();
	return 0;
}