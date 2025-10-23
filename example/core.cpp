#include <print>
#include <nagisa/concurrency/concurrency.h>
#include <ranges>
#include <algorithm>
#include <cassert>
#include <stdexec/execution.hpp>
#include <stdexec/coroutine.hpp>

namespace nc = ::nagisa::concurrency;
namespace nat = ::nc::awaitable_traits;

struct promise;

template<class Promise, class Parent>
using task_awaitable_trait = ::nc::awaitable_trait_combiner_t<Promise, Parent,
	::nat::ready_if_done

	, ::nat::capture_scheduler
	, ::nat::capture_stop_token
	, ::nat::this_then_parent
	, ::nat::run_this

	, ::nat::release_value
	, ::nat::rethrow_exception
	, ::nat::destroy_after_resumed
>;

template<class Promise, class Parent>
using task_awaitable = ::nc::awaitable_trait_instance_t<Promise, Parent, task_awaitable_trait>;
using task = ::nc::basic_task<promise, task_awaitable>;

struct promise
	: ::nc::promises::lazy
	, ::nc::promises::exception<true>
	, ::nc::promises::value<void>
	, ::nc::promises::jump_to_continuation<>
	, ::nc::promises::return_object_from_handle<promise, task>
	, ::nc::promises::schedulable<::stdexec::inline_scheduler>
	, ::nc::promises::stop_token
	, ::nc::promises::with_awaitable<promise>
{
	constexpr auto get_env() const noexcept
	{
		return ::stdexec::env(::nc::promises::schedulable<::stdexec::inline_scheduler>::get_env(), ::nc::promises::stop_token::get_env());
	}
};

static_assert(::nc::awaitable<task_awaitable<promise, void>>);
static_assert(::nc::awaitable<task>);

struct get_current_handle_t
{
	auto await_ready() const noexcept { return false; }
	::std::coroutine_handle<> result;
	auto await_suspend(::std::coroutine_handle<> parent) noexcept { result = parent;  return parent; }
	auto await_resume() const noexcept { return result; }
};

task f1(int i) noexcept
{
	::std::println("{}", i);
	auto handle = ::std::coroutine_handle<promise>::from_address((co_await get_current_handle_t{}).address());
	static_assert(::stdexec::__as_awaitable::__awaitable_sender<decltype(::stdexec::get_scheduler()), promise>);
	auto&& sche = co_await ::stdexec::get_scheduler();
	if (!i)
		co_return;
	co_await f1(i - 1);
}



int main()
{
	auto task = f1(5);
	task._coroutine.resume();
	assert(task._coroutine.done());

	return 0;
}