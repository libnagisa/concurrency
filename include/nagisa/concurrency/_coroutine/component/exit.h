#pragma once

/// @file exit.h
/// @brief Components controlling how a coroutine's frame is reclaimed.

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

namespace awaitable_traits
{
	/// @brief Awaitable trait: destroy the awaited coroutine's frame
	///        right after @c await_resume.
	///
	/// Pair with a promise that *does not* self-destroy on
	/// @c final_suspend (e.g. @c promises::default_exit), so the awaiter
	/// is in charge of freeing the frame.
	///
	/// @warning Once this trait runs, the coroutine handle is dangling.
	///          Do not reference it again from the awaiter.
	template<class Promise, class ParentPromise>
	struct destroy_after_resumed
	{
		using promise_type = Promise;
		using handle_type = ::std::coroutine_handle<promise_type>;
		constexpr static decltype(auto) await_resume(handle_type self) noexcept
		{
			self.destroy();
		}
	};
}

namespace promises
{
	/// @brief Promise mixin: @c final_suspend returns @c suspend_always.
	///
	/// The coroutine stops at @c final_suspend and waits for an external
	/// party to call @c destroy(). Use together with
	/// @c awaitable_traits::destroy_after_resumed when the awaiter is
	/// that external party.
	struct default_exit
	{
		constexpr static auto final_suspend() noexcept { return ::std::suspend_always{}; }
	};
	/// @brief Promise mixin: @c final_suspend destroys the coroutine frame.
	///
	/// "Fire-and-forget" termination — the frame is gone the moment the
	/// coroutine body finishes. Used by @c spawn_promise so that detached
	/// tasks clean up after themselves with no owner to call @c destroy().
	struct exit_then_destroy
	{
		constexpr static auto final_suspend() noexcept
		{
			struct awaitable
			{
				constexpr static auto await_ready() noexcept { return false; }
				static auto await_suspend(::std::coroutine_handle<> handle) noexcept { handle.destroy(); }
				[[noreturn]] static auto await_resume() noexcept { ::std::abort(); }
			};
			return awaitable{};
		}
	};
}

NAGISA_BUILD_LIB_DETAIL_END
