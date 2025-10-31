#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <memory>
#include <functional>
#include <coroutine>
#include <queue>

#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>
#include <exec/task.hpp>
#include <nagisa/concurrency/concurrency.h>


namespace nc = ::nagisa::concurrency;

::std::mutex mutex;
::nc::simple_task<void> print_id(auto sche, int id)
{
	for (auto i : ::std::views::iota(0, 3))
	{
		co_await ::stdexec::schedule(sche);
		::std::unique_lock l(mutex);
		::std::cout << "  [coro] id=" << id << " step " << i << " on thread " << std::this_thread::get_id() << '\n';
	}
	co_return;
}

::nc::simple_task<void> continue_on(auto awaitable, auto sche)
{
    co_await ::stdexec::schedule(sche);
    co_await ::std::move(awaitable);
}

template<class Scheduler, class Clock>
struct timer
{
	// using Scheduler = ::stdexec::inline_scheduler;
	// using Clock = ::std::chrono::steady_clock;
	using self_type = timer;
	using scheduler_type = Scheduler;
	using clock_type = Clock;
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
			if (next.due <= clock_type::now())
			{
				constexpr auto submit = [](::std::coroutine_handle<> handle, scheduler_type sche) noexcept -> ::nc::simple_task<void>
					{
						co_await ::stdexec::schedule(sche);
						handle.resume();
					};
				auto task = submit(next.handle, _other_scheduler).release();
				_tasks.pop();
				lk.unlock();
				task.resume();
				lk.lock();
				continue;
			}
			_cv.wait_until(lk, next.due);
		}
	}
	struct scheduler
	{
		self_type* _self;
		clock_type::duration _interval;
		struct awaitable
		{
			constexpr static auto await_ready() noexcept { return false; }
			auto await_suspend(::std::coroutine_handle<> parent) const noexcept
			{
				::std::unique_lock l(_self->_mutex);
				_self->_tasks.push({ parent, clock_type::now() + _interval });
				_self->_cv.notify_one();
			}
			constexpr static void await_resume() noexcept {}
			constexpr auto&& get_env() const noexcept { return *this; }
			template<class Tag>
			constexpr auto query(::stdexec::get_completion_scheduler_t<Tag>) const noexcept { return _self->get_scheduler(); }
			self_type* _self;
			clock_type::duration _interval;
		};
		constexpr auto schedule() const noexcept { return awaitable{ _self, _interval }; }
		constexpr bool operator==(const scheduler&) const = default;
	};
	constexpr auto get_scheduler(clock_type::duration i = {}) noexcept { return scheduler{ this, i }; }

	::stdexec::inplace_stop_source _stop_source;
	::std::mutex _mutex;
	::std::condition_variable _cv;
	struct t
	{
		::std::coroutine_handle<> handle;
		clock_type::time_point due;
		constexpr bool operator<(const t& other) const noexcept
		{
			return due > other.due;
		}
	};
	::std::priority_queue<t> _tasks{};
	scheduler_type _other_scheduler;
};

int main() {
	using namespace ::std::chrono_literals;

    auto num_workers = std::max(1u, std::thread::hardware_concurrency());
    std::cout << "starting static_thread_pool with " << num_workers << " workers\n";

    exec::static_thread_pool pool{ num_workers };
	timer<decltype(pool.get_scheduler()), ::std::chrono::steady_clock> timer{ ._other_scheduler = pool.get_scheduler() };
    auto handles
        = ::std::views::iota(0, 7)
        | ::std::views::transform([&](int id) { return ::print_id(timer.get_scheduler(1s), id); })
        | ::std::ranges::to<::std::vector>();
    for (auto&& h : handles)
    {
        h.handle().resume();
    }
	timer.run();
    std::cout << "main: requesting stop\n";
    pool.request_stop();
    return 0;
}