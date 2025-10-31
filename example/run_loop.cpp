#pragma once

#include <mutex>
#include <condition_variable>
#include <list>
#include <atomic>
#include <nagisa/concurrency/concurrency.h>
#include <stdexec/execution.hpp>
#include <coroutine>
#include <thread>
namespace details
{
	namespace nc = ::nagisa::concurrency;
	namespace nat = ::nagisa::concurrency::awaitable_traits;

	struct promise;
}
template<class... Args>
struct ::std::coroutine_traits<::std::coroutine_handle<details::promise>, Args...>
{
	using promise_type = details::promise;
};
namespace details
{
	struct run_loop
	{
		auto unhandled_exception(::std::exception_ptr ptr) noexcept
		{
			
		}

		constexpr auto add_task(auto&& awaitable);

		decltype(auto) request_stop() noexcept
		{
			return _stop_source.request_stop();
		}

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

			struct awaitable
			{
				constexpr static auto await_ready() noexcept { return false; }
				auto await_suspend(::std::coroutine_handle<> parent) const noexcept
				{
					_loop->_push_back(parent);
				}
				constexpr static void await_resume() noexcept {}

				constexpr auto&& get_env() const noexcept { return *this; }

				template<class Tag>
				constexpr auto query(::stdexec::get_completion_scheduler_t<Tag>) const noexcept
				{
					return _loop->get_scheduler();
				}

				run_loop* _loop;
			};
			constexpr auto schedule() const noexcept
			{
				return awaitable{ _loop };
			}

			auto unhandled_exception(::std::exception_ptr ptr) const noexcept
			{
				_loop->unhandled_exception(ptr);
			}
			constexpr bool operator==(const scheduler&) const = default;
		};

		constexpr auto get_scheduler() noexcept
		{
			return scheduler{ this };
		}

		::stdexec::inplace_stop_source _stop_source;
		::std::mutex _mutex;
		::std::condition_variable _cv;
		::std::list<::std::coroutine_handle<>> _tasks;
	};

	struct promise
		: nc::promises::lazy
		, nc::promises::value<void>
		, nc::promises::exit_then_destroy
		, nc::promises::return_object_from_handle<promise, ::std::coroutine_handle<promise>>
		, nc::promises::with_scheduler<run_loop::scheduler>
		, nc::promises::with_stop_token<>
		, nc::promises::use_as_awaitable<promise>
	{
		[[nodiscard]] auto unhandled_exception() const noexcept
		{
			auto&& scheduler = ::stdexec::get_scheduler(with_scheduler::get_env());
			scheduler.unhandled_exception(::std::current_exception());
		}
		constexpr auto get_env() const noexcept
		{
			return ::stdexec::env{ with_scheduler::get_env(), with_stop_token::get_env() };
		}
	};

	static ::std::coroutine_handle<promise> wrapper(auto awaitable) noexcept
	{
		co_await ::std::forward<decltype(awaitable)>(awaitable);
	}

	constexpr auto run_loop::add_task(auto&& awaitable)
	{
		//static_assert(nc::awaitable<decltype(awaitable), promise>);

		auto handle = wrapper(::std::forward<decltype(awaitable)>(awaitable));
		nc::set_stop_token(handle.promise(), _stop_source.get_token());
		run_loop::_push_back(handle);
	}

}


namespace nc = ::nagisa::concurrency;
namespace nat = ::nc::awaitable_traits;

struct promise;

template<class Promise, class Parent>
using task_awaitable_trait = ::nc::awaitable_trait_combiner_t<Promise, Parent,
	::nat::ready_if_done

	, ::nat::capture_scheduler
	, ::nat::capture_inplace_stop_token
	, ::nat::this_then_parent
	, ::nat::run_this

	, ::nat::release_value
	, ::nat::rethrow_exception
	, ::nat::destroy_after_resumed
>;

template<class Promise, class Parent>
using trait_instance = ::nc::awaitable_trait_instance_t<task_awaitable_trait, Promise, Parent>;

using task = ::nc::basic_task<promise, trait_instance>;

struct promise
	: ::nc::promises::lazy
	, ::nc::promises::exception<true>
	, ::nc::promises::value<void>
	, ::nc::promises::jump_to_continuation<>
	, ::nc::promises::return_object_from_handle<promise, task>
	, ::nc::promises::with_scheduler<>
	, ::nc::promises::with_stop_token<>
	, ::nc::promises::use_as_awaitable<promise>
{
	constexpr auto get_env() const noexcept
	{
		return ::stdexec::env(
			with_scheduler::get_env()
			, with_stop_token::get_env()
		);
	}
};

task g(int id)
{
	// ::std::println("g1 {}", id);
	co_await ::stdexec::schedule(co_await ::stdexec::get_scheduler());
	// ::std::println("g2 {}", id);
}

task f(details::run_loop::scheduler s, int id)
{
	// ::std::println("f1 {}", id);
	co_await g(id);
	// ::std::println("f2 {}", id);
	co_return;
}
#include <exec/static_thread_pool.hpp>
::exec::static_thread_pool;
int main()
{
	details::run_loop loop{};
	loop.add_task(f(loop.get_scheduler(), 0));
	loop.add_task(f(loop.get_scheduler(), 1));
	loop.add_task(f(loop.get_scheduler(), 2));
	auto _ = ::std::jthread([&] {loop.run(); });
	::std::this_thread::sleep_for(::std::chrono::seconds(1));
	loop.request_stop();
	return 0;
}