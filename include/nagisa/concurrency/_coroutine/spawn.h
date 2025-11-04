#pragma once

#include "./awaitable.h"
#include "./task.h"
#include "./component/intro.h"
#include "./component/exception.h"
#include "./component/value.h"
#include "./component/return_object.h"
#include "./component/exit.h"
#include "./component/with_awaitable.h"
#include "./component/stop_token.h"
#include "./current_handle.h"
#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

struct spawn_promise;
using spawn_task = basic_task<spawn_promise>;

struct spawn_promise
    : promises::eager
	, promises::return_object_from_handle<spawn_promise, spawn_task>
    , promises::exception<true>
    , promises::value<void>
	, promises::exit_then_destroy
	, promises::with_stop_token<>
	, promises::use_as_awaitable<spawn_promise>
{
};

template<intro_type Intro>
spawn_task spawn_impl(::stdexec::scheduler auto scheduler, auto&& task) noexcept
{
	// task maybe destroyed after co_await schedule, so we need to move it
    auto task_awaitable = ::stdexec::as_awaitable(::std::forward<decltype(task)>(task), (co_await details::current_handle()).promise());
	if constexpr (Intro == intro_type::lazy)
	{
		co_await ::std::suspend_always{};
	}
	co_await ::stdexec::schedule(scheduler);
	co_await ::std::move(task_awaitable);
}

template<intro_type Intro = intro_type::eager>
constexpr auto spawn(::stdexec::scheduler auto&& scheduler, awaitable<spawn_promise> auto&& task) noexcept
{
	return details::spawn_impl<Intro>(::std::forward<decltype(scheduler)>(scheduler), ::std::forward<decltype(task)>(task)).release();
}

NAGISA_BUILD_LIB_DETAIL_END