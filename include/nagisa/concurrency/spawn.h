#pragma once

#include <nagisa/concurrency/coroutine.h>
#include <nagisa/concurrency/environment.h>

#if NAGISA_CONCURRENCY_USE_EXECUTION

NAGISA_BUILD_LIB_DETAIL_BEGIN

struct spawn_promise;
using spawn_task = basic_task<spawn_promise>;

struct spawn_promise
    : promises::eager
	, promises::return_object_from_handle<spawn_promise, spawn_task>
    , promises::exception<true>
    , promises::value<void>
	, promises::exit_then_destroy
	, promises::without_stop_token
	, promises::with_await_transform<spawn_promise>
{};

template<intro_type Intro>
spawn_task spawn_impl(::stdexec::scheduler auto scheduler, auto&& task) noexcept
{
	// for gcc(ver < 13)
	::std::coroutine_handle<spawn_promise> handle = co_await details::current_handle();
	// task maybe destroyed after co_await schedule, so we need to move it
    auto task_awaitable = ::stdexec::as_awaitable(::std::forward<decltype(task)>(task), handle.promise());
	if constexpr (Intro == intro_type::lazy)
	{
		co_await ::std::suspend_always{};
	}
	co_await ::stdexec::schedule(scheduler);
	// forward like
	if constexpr (::std::is_rvalue_reference_v<decltype(task)>)
		co_await ::std::move(task_awaitable);
	else
		co_await task_awaitable;
}

template<intro_type Intro = intro_type::eager>
constexpr auto spawn(::stdexec::scheduler auto&& scheduler, awaitable<spawn_promise> auto&& task) noexcept
{
	return details::spawn_impl<Intro>(::std::forward<decltype(scheduler)>(scheduler), ::std::forward<decltype(task)>(task)).release();
}

NAGISA_BUILD_LIB_DETAIL_END

NAGISA_BUILD_LIB_BEGIN

using details::spawn;

NAGISA_BUILD_LIB_END

#endif

#include <nagisa/build_lib/destruct.h>