#pragma once

/// @file intro.h
/// @brief "Start-up" components: lazy/eager promises and the matching
///        @c await_ready traits.
///
/// A coroutine's start-up has two related decisions:
///   - When the coroutine is *created*, should it run immediately or
///     stay suspended until someone calls @c handle.resume()?
///     → controlled by the promise's @c initial_suspend.
///   - When the coroutine is *awaited*, should @c co_await skip the
///     suspension altogether (because we already have a result)?
///     → controlled by the awaiter's @c await_ready.

#include "./environment.h"


NAGISA_BUILD_LIB_DETAIL_BEGIN

/// @brief Tag describing whether a task starts running immediately on construction.
///
/// Used as a template argument to @ref simple_task and @ref spawn to
/// switch between @c lazy / @c eager promise behavior.
enum class intro_type
{
	lazy,   ///< Initial suspend is @c suspend_always: task waits to be resumed.
	eager,  ///< Initial suspend is @c suspend_never: task runs immediately on construction.
};

namespace awaitable_traits
{
	/// @brief Awaitable trait: @c await_ready always returns @c false.
	///
	/// Use when the awaiter must always perform the suspend dance — for
	/// example, if @c await_suspend is responsible for kicking the
	/// coroutine onto a scheduler.
	template<class Promise, class>
	struct ready_false
	{
		using promise_type = Promise;
		using handle_type = ::std::coroutine_handle<promise_type>;

		constexpr static decltype(auto) await_ready(handle_type handle) noexcept
		{
			return false;
		}
	};
	/// @brief Awaitable trait: @c await_ready always returns @c true.
	///
	/// Use when the result is already available at the time of @c co_await —
	/// the awaiter skips suspension and @c await_resume runs synchronously.
	template<class Promise, class>
	struct ready_true
	{
		using promise_type = Promise;
		using handle_type = ::std::coroutine_handle<promise_type>;

		constexpr static decltype(auto) await_ready(handle_type handle) noexcept
		{
			return true;
		}
	};

	/// @brief Awaitable trait: @c await_ready returns @c handle.done().
	///
	/// The common "if the eager task already finished, don't suspend"
	/// pattern. Used by @ref simple_task.
	template<class Promise, class>
	struct ready_if_done
	{
		using promise_type = Promise;
		using handle_type = ::std::coroutine_handle<promise_type>;

		constexpr static decltype(auto) await_ready(handle_type handle) noexcept
		{
			return handle.done();
		}
	};
}

namespace promises
{
	/// @brief Promise mixin: @c initial_suspend returns @c suspend_always.
	///
	/// The coroutine is created suspended; nothing runs until something
	/// (the caller, a scheduler, @c co_await) resumes it.
	struct lazy
	{
		constexpr static auto initial_suspend() noexcept { return ::std::suspend_always{}; }
	};

	/// @brief Promise mixin: @c initial_suspend returns @c suspend_never.
	///
	/// The coroutine starts executing as part of its return-object
	/// construction (typically up to the first @c co_await).
	struct eager
	{
		constexpr static auto initial_suspend() noexcept { return ::std::suspend_never{}; }
	};
}

NAGISA_BUILD_LIB_DETAIL_END
