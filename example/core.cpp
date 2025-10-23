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
	, ::nc::promises::jump_to_continuation
	, ::nc::promises::return_object_from_handle<promise, task>
	, ::nc::promises::schedulable<::stdexec::inline_scheduler>
	, ::nc::promises::with_awaitable<promise>
{
};

static_assert(::nc::awaitable<task_awaitable<promise, void>>);
static_assert(::nc::awaitable<task>);

task f1(int i) noexcept
{
	::std::println("{}", i);
	// auto&& sche = co_await ::stdexec::as_awaitable(::stdexec::get_scheduler(), );
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