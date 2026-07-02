#pragma once

/// @file spawn.h
/// @brief @ref spawn — start a coroutine in detached fashion on a scheduler.
///
/// The spawned coroutine takes responsibility for its own destruction
/// (its promise uses @c exit_then_destroy), so the caller doesn't need
/// to hold onto a handle. Exceptions are *captured* but not delivered
/// to anyone — if the task throws, the exception will be silently
/// discarded when the frame is destroyed. Use a different coroutine
/// type if you need observability.

#include <nagisa/concurrency/coroutine.h>
#include <nagisa/concurrency/environment.h>
#include <iostream>

#if NAGISA_CONCURRENCY_USE_EXECUTION

NAGISA_BUILD_LIB_DETAIL_BEGIN

struct spawn_promise;

/// @brief Coroutine return type used internally by @ref spawn.
///
/// Just a @ref basic_task wrapping @ref spawn_promise — the
/// awaitable parameter is the default no-op, so the task itself isn't
/// directly @c co_await'able; it is constructed only to be immediately
/// @c release'd by @ref spawn.
using spawn_task = basic_task<spawn_promise>;

/// @brief Fire-and-forget promise: eager start, self-destroys on completion.
///
/// Component breakdown:
///   - @c eager — runs as part of construction,
///   - @c return_object_from_handle — synthesizes @c get_return_object,
///   - @c exception<true> — captures unhandled exceptions (but no one
///     reads them; effectively swallowed when the frame is destroyed),
///   - @c value<void> — no return value,
///   - @c exit_then_destroy — destroys the frame at @c final_suspend,
///   - @c without_stop_token — cancellation is not modeled,
///   - @c with_await_transform — sender support inside the spawned body.
struct spawn_promise
    : promises::eager
	, promises::return_object_from_handle<spawn_promise, spawn_task>
    , promises::exception<true>
    , promises::value<void>
	, promises::exit_then_destroy
	, promises::without_stop_token
	, promises::with_await_transform<spawn_promise>
{
};

/// @internal Implementation of @ref spawn. Takes ownership of @p task,
/// optionally suspends once for the @c lazy variant, then awaits the
/// task on @p scheduler.
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

/// @brief Start a coroutine in detached fashion on @p scheduler.
///
/// The returned value is the bare coroutine handle (already released
/// from any owning task). The default @p Intro is @c eager: the
/// spawned coroutine begins running synchronously as part of this
/// call, up to its first suspension. Pass @c intro_type::lazy if you
/// want the spawn to be queued without running until something else
/// drives the scheduler.
///
/// @warning Exceptions thrown by @p task are silently discarded —
///          there is no awaiter to receive them. Use a different
///          coroutine type if you need to observe failure.
///
/// @tparam Intro Start-up mode for the spawn driver coroutine.
/// @param scheduler The scheduler on which @p task should be resumed.
/// @param task      An awaitable to run; will be moved/captured.
/// @return The released coroutine handle of the spawn driver.
///
/// @code
///   // From example/run_loop.cpp:
///   nc::spawn(loop.get_scheduler(), f(0));
///   nc::spawn(loop.get_scheduler(), f(1));
///   nc::spawn(loop.get_scheduler(), f(2));
/// @endcode
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
