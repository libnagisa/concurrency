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
	/// Use this only for **detached / fire-and-forget** coroutines that own
	/// their frame exclusively. Once the body finishes, @c final_suspend
	/// destroys the frame immediately; there must be no outer awaiter /
	/// opstate that still expects to call @c await_resume or @c destroy().
	///
	/// Correct:
	/// - Internal driver promises such as @c spawn_promise.
	/// - Other self-owned detached coroutines whose handle is released and
	///   never observed again.
	///
	/// Incorrect:
	/// - Ordinary tasks meant to be @c co_await'ed or connected as senders.
	/// - Promises paired with @c awaitable_traits::destroy_after_resumed
	///   (that combination double-frees).
	/// - Any protocol that still needs the frame after completion to deliver
	///   a value/exception, resume a parent, or run sticky/affinity logic.
	///
	/// Note: @c spawn uses this on its **internal driver** promise, not as a
	/// general rule for every user task that happens to be spawned.
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
