#include <nagisa/concurrency/concurrency.h>
#include <ranges>
#include <algorithm>
#include <cassert>
#include <any>
#include <stdexec/execution.hpp>
#include <stdexec/coroutine.hpp>
#include <exec/static_thread_pool.hpp>

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
		return ::stdexec::env(::nc::promises::with_scheduler<>::get_env(), ::nc::promises::with_stop_token<>::get_env());
	}
};
static_assert(::nc::awaitable<trait_instance<promise, void>>);
static_assert(::nc::awaitable<task>);

struct get_current_handle_t
{
	auto await_ready() const noexcept { return false; }
	::std::coroutine_handle<> result;
	auto await_suspend(::std::coroutine_handle<> parent) noexcept { result = parent;  return parent; }
	auto await_resume() const noexcept { return result; }
};

struct check_stop_t
{
	auto await_ready() const noexcept { return false; }
	template<class Promise>
	::std::coroutine_handle<> await_suspend(::std::coroutine_handle<Promise> parent) noexcept
	{
		if constexpr (requires { ::stdexec::get_stop_token(::stdexec::get_env(parent.promise())); })
		{
			static_assert(requires{ { parent.promise().unhandled_stopped() }; });
			if (::stdexec::get_stop_token(::stdexec::get_env(parent.promise())).stop_requested())
				return parent.promise().unhandled_stopped();
		}
		return parent;
	}
	auto await_resume() const noexcept { }
};

task f1(int i) noexcept
{
	// ::std::println("{}", i);
	co_await check_stop_t{};
	auto&& sche = co_await ::stdexec::get_scheduler();
	co_await sche.schedule();
	if (!i)
		co_return;
	co_await f1(i - 1);
	co_return;
}

int main()
{
	auto task = f1(5).operator co_await();
	if (!task.await_ready())
	{
		auto handle = task.await_suspend(::std::noop_coroutine());
		handle.resume();
	}
	task.await_resume();
	assert(task._coroutine.done());

	return 0;
}