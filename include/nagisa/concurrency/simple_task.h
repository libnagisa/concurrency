#pragma once

/// @file simple_task.h
/// @brief @ref simple_task — a batteries-included coroutine type
///        assembled from the library's component set.
///
/// This is the recommended starting point for users who want a
/// general-purpose coroutine return type that:
///   - is lazy by default (configurable),
///   - propagates exceptions back to the awaiter (configurable),
///   - carries a scheduler and a stop token, both auto-captured from
///     the parent when awaited,
///   - is interoperable with stdexec (any sender can be @c co_await'ed
///     from inside it; the task itself satisfies @c stdexec::sender).
///
/// If you need different behavior, see docs/07_custom_task.md for how
/// to assemble your own using the component library directly.

#include <nagisa/concurrency/coroutine.h>
#include <nagisa/concurrency/environment.h>

#if NAGISA_CONCURRENCY_USE_EXECUTION

NAGISA_BUILD_LIB_DETAIL_BEGIN

template<class Value, bool Throw, class Scheduler, intro_type Intro, class StopToken, class Handle>
struct simple_promise;

/// @brief The awaiter type used by @ref simple_task — assembled from
///        the canonical trait set.
///
/// On @c co_await:
///   - if the child is already done, skip suspension (@c ready_if_done),
///   - capture the parent's scheduler (@c capture_scheduler) and stop
///     token (@c capture_inplace_stop_token) into the child,
///   - wire the parent as the child's continuation (@c this_then_parent),
///   - symmetric-transfer into the child (@c run_this),
///   - on resume, release the child's return value (@c release_value),
///     rethrow any captured exception (@c rethrow_exception),
///     and destroy the child's frame (@c destroy_after_resumed).
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

/// @brief A ready-to-use coroutine return type.
///
/// @tparam Value     The @c co_return value type. Use @c void for
///                   coroutines that don't return a value.
/// @tparam Throw     If @c true, unhandled exceptions are captured and
///                   rethrown into the awaiter. If @c false, they call
///                   @c std::abort — only safe for @c noexcept coroutine bodies.
/// @tparam Scheduler The scheduler type carried by the promise.
///                   Defaults to @c stdexec::inline_scheduler.
/// @tparam Intro     Start-up mode: @c lazy (default) waits to be resumed;
///                   @c eager runs immediately on construction.
/// @tparam StopToken The stop-token type carried by the promise.
///                   Defaults to @c stdexec::inplace_stop_token.
/// @tparam Handle    The continuation handle type stored in the promise.
///                   Defaults to the erased @c std::coroutine_handle<>.
///
/// @code
///   simple_task<int> compute() {
///       co_await stdexec::schedule(co_await stdexec::get_scheduler());
///       co_return 42;
///   }
///
///   // From another coroutine:
///   int x = co_await compute();
///
///   // Or as a sender:
///   auto [x] = stdexec::sync_wait(compute()).value();
/// @endcode
template<
	class Value
	, bool Throw = true
	, class Scheduler = ::stdexec::inline_scheduler
	, intro_type Intro = intro_type::lazy
	, class StopToken = ::stdexec::inplace_stop_token
	, class Handle = ::std::coroutine_handle<>
>
using simple_task = basic_task<simple_promise<Value, Throw, Scheduler, Intro, StopToken, Handle>, simple_awaitable>;

/// @brief The promise type used by @ref simple_task.
///
/// Inherits all of:
///   - @c lazy or @c eager (depending on @c Intro),
///   - @c exception<Throw>,
///   - @c value<Value>,
///   - @c jump_to_continuation<Handle>,
///   - @c return_object_from_handle to synthesize @c get_return_object,
///   - @c with_scheduler<Scheduler>,
///   - @c with_stop_token<StopToken>,
///   - @c with_await_transform so any sender can be @c co_await'ed.
///
/// See @ref simple_task for the user-facing template parameter docs;
/// see docs/05_components.md for the role of each base.
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

	/// @brief Constructs from a stdexec env-providing first argument.
	///
	/// Allows promise initialization from a connect environment, so
	/// scheduler / stop token can flow in from the caller's
	/// @c stdexec::operation_state.
	constexpr explicit(false) simple_promise(auto const& env, auto&&...)
		noexcept(::std::is_nothrow_constructible_v<base_sche_type, decltype(env)> && ::std::is_nothrow_constructible_v<base_st_type, decltype(env)>)
		requires ::std::constructible_from<base_sche_type, decltype(env)> && ::std::constructible_from<base_st_type, decltype(env)>
		: base_sche_type(env)
		, base_st_type(env)
	{}

	/// @brief Composite env exposing both scheduler and stop_token queries.
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
