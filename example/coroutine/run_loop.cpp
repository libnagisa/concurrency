#pragma once

#include <mutex>
#include <condition_variable>
#include <list>
#include <nagisa/concurrency/concurrency.h>
#include <stdexec/execution.hpp>
#include <coroutine>
#include <thread>
#include <iostream>

namespace nc = ::nagisa::concurrency;

struct run_loop
{
	decltype(auto) request_stop() noexcept { return _stop_source.request_stop(); }
	auto _push_back(::std::coroutine_handle<> handle)
	{
		::std::unique_lock lock{ _mutex };
		_tasks.push_back(handle);
		_cv.notify_one();
	}
	auto _pop_front()
	{
		::std::unique_lock lock{ _mutex };
		_cv.wait(lock, [this] { return !_tasks.empty(); });
		auto handle = _tasks.front();
		_tasks.pop_front();
		return handle;
	}
	auto run() noexcept
	{
		while (!_stop_source.stop_requested())
		{
			auto handle = _pop_front();
			handle.resume();
		}
	}
	struct scheduler
	{
		run_loop* _loop;
		struct awaitable : ::std::suspend_always
		{
			auto await_suspend(::std::coroutine_handle<> parent) const noexcept { _loop->_push_back(parent); }
			constexpr auto&& get_env() const noexcept { return *this; }
			template<class Tag>
			constexpr auto query(::stdexec::get_completion_scheduler_t<Tag>) const noexcept { return scheduler{_loop}; }

			run_loop* _loop;
		};
		constexpr auto schedule() const noexcept { return awaitable{ {}, _loop }; }
		constexpr bool operator==(const scheduler&) const = default;
	};
	constexpr auto get_scheduler() noexcept { return scheduler{ this }; }

	::stdexec::inplace_stop_source _stop_source;
	::std::mutex _mutex;
	::std::condition_variable _cv;
	::std::list<::std::coroutine_handle<>> _tasks;
};
::nc::simple_task<void> g(int id)
{
	::std::cout << "g1 " << id << ::std::endl;
	co_await ::stdexec::schedule(co_await ::stdexec::get_scheduler());
	::std::cout << "g2 " << id << ::std::endl;
}
::nc::simple_task<void> f(int id)
{
	::std::cout << "f1 " << id << ::std::endl;
	co_await g(id);
	::std::cout << "f2 " << id << ::std::endl;
}

int main()
{
	run_loop loop{};
	::nc::spawn(loop.get_scheduler(), f(0));
	::nc::spawn(loop.get_scheduler(), f(1));
	::nc::spawn(loop.get_scheduler(), f(2));
	auto _ = ::std::jthread([&] { loop.run(); });
	::std::this_thread::sleep_for(::std::chrono::seconds(1));
	loop.request_stop();
	return 0;
}