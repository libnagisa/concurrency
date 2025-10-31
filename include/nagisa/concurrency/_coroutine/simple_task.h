#pragma once

#include "./awaitable_trait.h"
#include "./component/intro.h"
#include "./component/with_scheduler.h"
#include "./component/stop_token.h"
#include "./component/workflow.h"
#include "./component/exit.h"
#include "./component/exception.h"
#include "./component/value.h"
#include "./component/with_awaitable.h"
#include "./task.h"
#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

enum class intro_type
{
    lazy,
    eager,
};

template<class Value, bool Throw, class Scheduler, intro_type Intro, class StopToken, class Handle>
struct simple_promise;

template<
    class Value
	, bool Throw = true
	, class Scheduler = ::stdexec::inline_scheduler
	, intro_type Intro = intro_type::lazy
	, class StopToken = ::stdexec::inplace_stop_token
	, class Handle = ::std::coroutine_handle<>
>
using simple_task = make_basic_task_with_trait_t< simple_promise<Value, Throw, Scheduler, Intro, StopToken, Handle>
    , awaitable_traits::ready_if_done
    , awaitable_traits::capture_scheduler
    , awaitable_traits::capture_inplace_stop_token
    , awaitable_traits::this_then_parent
    , awaitable_traits::run_this
    , awaitable_traits::release_value
    , awaitable_traits::rethrow_exception
    , awaitable_traits::destroy_after_resumed
>;

template<class Value, bool Throw, class Scheduler, intro_type Intro, class StopToken, class Handle>
struct simple_promise
    : ::std::conditional_t<Intro == intro_type::eager, promises::eager, promises::lazy>
    , promises::exception<Throw>
    , promises::value<Value>
    , promises::jump_to_continuation<Handle>
    , promises::return_object_from_handle<
		simple_promise<Value, Throw, Scheduler, Intro, StopToken, Handle>
		, simple_task<Value, Throw, Scheduler, Intro, StopToken, Handle>>
    , promises::with_scheduler<Scheduler>
    , promises::with_stop_token<StopToken>
    , promises::use_as_awaitable<simple_promise<Value, Throw, Scheduler, Intro, StopToken, Handle>>
{
    constexpr auto get_env() const noexcept
    {
        return ::stdexec::env(promises::with_scheduler<Scheduler>::get_env(), promises::with_stop_token<StopToken>::get_env());
    }
};

NAGISA_BUILD_LIB_DETAIL_END