#include <nagisa/concurrency/concurrency.h>
#include <stdexec/execution.hpp>
#include <iostream>

namespace nc = ::nagisa::concurrency;

struct promise;

template<class Promise, class Parent>
using awaitable = ::nc::build_awaitable_t<Promise, Parent
	, ::nc::awaitable_traits::ready_if_done
	, ::nc::awaitable_traits::capture_scheduler
	, ::nc::awaitable_traits::capture_inplace_stop_token
	, ::nc::awaitable_traits::this_then_parent
	, ::nc::awaitable_traits::run_this
	, ::nc::awaitable_traits::release_value
	, ::nc::awaitable_traits::rethrow_exception
	, ::nc::awaitable_traits::destroy_after_resumed
>;

using task = ::nc::basic_task<promise, awaitable>;

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
		return ::stdexec::env(::nc::promises::with_scheduler<>::get_env(), ::nc::promises::with_stop_token<>::get_env());
	}
};
static_assert(::nc::awaitable<awaitable<promise, void>>);
static_assert(::nc::awaitable<task>);

struct check_stop_t : ::std::suspend_always
{
	template<class Promise>
	static ::std::coroutine_handle<> await_suspend(::std::coroutine_handle<Promise> parent) noexcept
	{
		if constexpr (requires { ::stdexec::get_stop_token(::stdexec::get_env(parent.promise())); })
		{
			static_assert(requires{ { parent.promise().unhandled_stopped() }; });
			if (::stdexec::get_stop_token(::stdexec::get_env(parent.promise())).stop_requested())
				return parent.promise().unhandled_stopped();
		}
		return parent;
	}
};
constexpr auto check_stop() noexcept { return check_stop_t{}; }
task f1(int i) noexcept
{
	::std::cout << i << ::std::endl;
	co_await ::check_stop();
	co_await ::stdexec::schedule(co_await ::stdexec::get_scheduler());
	if (!i)
		co_return;
	co_await f1(i - 1);
}

int main()
{
	{
		auto t = f1(5);
		// lazy start
		t.handle().resume();
		// coroutine will be destroyed when task destruct
		// t.~task();
	}
	{
		auto t = f1(5);
		// lazy start
		t.handle().resume();
		// release the ownership
		auto h = t.release();
		// must destroy the coroutine
		h.destroy();
	}
	{
		::nc::spawn(::stdexec::inline_scheduler{}, f1(5));
	}
	return 0;
}