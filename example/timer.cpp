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
#include <exec/timed_thread_scheduler.hpp>


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
	using task_type = timer_task_t<typename clock_type::time_point>;

	decltype(auto) request_stop() noexcept { return _stop_source.request_stop(); }
	auto run() noexcept
	{
		auto lk = ::std::unique_lock(_mutex);
		while (!_stop_source.stop_requested())
		{
			if (_tasks.empty())
			{
				_cv.wait(lk, [this] { return _stop_source.stop_requested() || !_tasks.empty(); });
				continue;
			}
			auto& next = _tasks.top();
			auto deadline = next.due;
			if (deadline > clock_type::now())
			{
				_cv.wait_until(lk, deadline);
				continue;
			}
			auto handle = next.handle;
			_tasks.pop();
			lk.unlock();
			::nc::spawn(_other_scheduler, ::nc::sync_invoke([handle] { handle.resume(); }));
			lk.lock();
		}
	}
	constexpr auto now() const noexcept { return clock_type::now(); }
	struct scheduler
	{
		self_type* _self;
		struct awaitable_base
		{
			constexpr static auto await_ready() noexcept { return false; }
			constexpr static void await_resume() noexcept {}
			constexpr auto&& get_env() const noexcept { return *this; }
			template<class Tag>
			constexpr auto query(::stdexec::get_completion_scheduler_t<Tag>) const noexcept { return _self->get_scheduler(); }
			self_type* _self;
		};
		struct at : awaitable_base
		{
			auto await_suspend(::std::coroutine_handle<> parent) const noexcept
			{
				::std::unique_lock l(awaitable_base::_self->_mutex);
				awaitable_base::_self->_tasks.push({ parent, _due });
				awaitable_base::_self->_cv.notify_one();
			}
			typename clock_type::time_point _due;
		};
		struct delay : awaitable_base
		{
			auto await_suspend(::std::coroutine_handle<> parent) const noexcept
			{
				::std::unique_lock l(awaitable_base::_self->_mutex);
				awaitable_base::_self->_tasks.push({ parent, clock_type::now() + _interval });
				awaitable_base::_self->_cv.notify_one();
			}
			typename clock_type::duration _interval;
		};
		constexpr auto schedule() const noexcept { return delay{ _self, {} }; }
		constexpr auto schedule_at(typename clock_type::time_point tp) const noexcept { return at{ _self, tp }; }
		constexpr auto schedule_after(typename clock_type::duration interval) const noexcept { return delay{ _self, interval }; }
		constexpr bool operator==(const scheduler&) const = default;
	};
	constexpr auto get_scheduler() noexcept { return scheduler{ this }; }

	timer(scheduler_type scheduler) : _other_scheduler(scheduler) {}

	::stdexec::inplace_stop_source _stop_source{};
	::std::mutex _mutex{};
	::std::condition_variable _cv{};
	::std::priority_queue<task_type, ::std::vector<task_type>, ::std::greater<>> _tasks{};
	scheduler_type _other_scheduler;
};
template<class Scheduler>
timer(Scheduler) -> timer<Scheduler>;

::std::mutex mutex;
::nc::simple_task<void> print_id(auto sche, ::stdexec::inplace_stop_token stop_token, int id)
{
	while (!stop_token.stop_requested())
	{
		co_await sche.schedule_after(::std::chrono::milliseconds(100));
		::std::unique_lock l(mutex);
		::std::cout << "  [coro] id=" << id << " on thread " << std::this_thread::get_id() << '\n';
	}
	co_return;
}

int main() {
	using namespace ::std::chrono_literals;

	auto num_workers = std::max(1u, std::thread::hardware_concurrency());
	std::cout << "starting static_thread_pool with " << num_workers << " workers\n";
	
	exec::static_thread_pool pool{ num_workers };
	timer timer(pool.get_scheduler());
	static_assert(::nc::awaitable<::nc::simple_task<void>, ::nc::details::spawn_promise>);
	for (auto id : ::std::views::iota(0, 7))
	{
		::nc::spawn(pool.get_scheduler(), ::print_id(timer.get_scheduler(), timer._stop_source.get_token(), id));
	}
	// ::nc::spawn(pool.get_scheduler(), ::nc::sync_invoke([&] { timer.run(); }));
	::std::jthread _([&] { timer.run();  });
	::std::this_thread::sleep_for(10s);
	std::cout << "main: requesting stop\n";
	timer.request_stop();
	pool.request_stop();
	return 0;
}