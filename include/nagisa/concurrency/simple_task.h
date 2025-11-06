#pragma once

#include <nagisa/concurrency/coroutine.h>
#include <nagisa/concurrency/environment.h>

#if NAGISA_CONCURRENCY_USE_EXECUTION

NAGISA_BUILD_LIB_DETAIL_BEGIN

template<class Value, bool Throw, class Scheduler, intro_type Intro, class StopToken, class Handle>
struct simple_promise;

template<class Promise, class Parent>
using simple_awaitable = build_awaitable_t<Promise, Parent
	, awaitable_traits::ready_if_done
	, awaitable_traits::capture_scheduler
	, awaitable_traits::capture_inplace_stop_token
	, awaitable_traits::this_then_parent
	, awaitable_traits::run_this
	, awaitable_traits::release_value
	, awaitable_traits::rethrow_exception
	, awaitable_traits::destroy_after_resumed
>;

template<
	class Value
	, bool Throw = true
	, class Scheduler = ::stdexec::inline_scheduler
	, intro_type Intro = intro_type::lazy
	, class StopToken = ::stdexec::inplace_stop_token
	, class Handle = ::std::coroutine_handle<>
>
using simple_task = basic_task<simple_promise<Value, Throw, Scheduler, Intro, StopToken, Handle>, simple_awaitable>;

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
	, promises::with_await_transform<simple_promise<Value, Throw, Scheduler, Intro, StopToken, Handle>>
{
private:
	using base_sche_type = promises::with_scheduler<Scheduler>;
	using base_st_type = promises::with_stop_token<StopToken>;
public:
	constexpr explicit(false) simple_promise()
		noexcept(::std::is_nothrow_default_constructible_v<base_sche_type>&& ::std::is_nothrow_default_constructible_v<base_st_type>)
		requires ::std::default_initializable<base_sche_type>&& ::std::default_initializable<base_st_type>
	= default;

	constexpr explicit(false) simple_promise(auto const& env, auto&&...)
		noexcept(::std::is_nothrow_constructible_v<base_sche_type, decltype(env)> && ::std::is_nothrow_constructible_v<base_st_type, decltype(env)>)
		requires ::std::constructible_from<base_sche_type, decltype(env)> && ::std::constructible_from<base_st_type, decltype(env)>
		: base_sche_type(env)
		, base_st_type(env)
	{}

	constexpr auto get_env() const noexcept
	{
		return ::stdexec::env(base_sche_type::get_env(), base_st_type::get_env());
	}
};

NAGISA_BUILD_LIB_DETAIL_END

NAGISA_BUILD_LIB_BEGIN

using details::simple_task;
using details::simple_awaitable;
using details::simple_promise;

NAGISA_BUILD_LIB_END

#endif

#include <nagisa/build_lib/destruct.h>