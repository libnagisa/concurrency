#pragma once

/// @file workflow.h
/// @brief Awaitable traits that orchestrate control flow on @c await_suspend:
///        who runs next, and how the child's continuation is wired to the parent.

#include "./continuation.h"
#include "./environment.h"

#include "../macro_push.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN
	namespace awaitable_traits
{
	/// @brief Awaitable trait: @c await_suspend is a no-op.
	///
	/// Effectively a "void" placeholder so a trait can declare it
	/// participates in @c await_suspend without doing anything; useful
	/// only when combined with other traits via @c awaitable_trait_combiner.
	template<class Promise, class ParentPromise>
	struct default_suspend
	{
		constexpr static NAGISA_CONCURRENCY_WD4100_INSTANTIATION_ICE(void, decltype(auto)) await_suspend(::std::coroutine_handle<Promise> self, ::std::coroutine_handle<ParentPromise> parent) noexcept {}
	};

	/// @brief Awaitable trait: symmetric transfer to the awaited (child) coroutine.
	///
	/// @c await_suspend returns @c self, so the C++20 coroutine machinery
	/// jumps straight into the child without going back to the resume
	/// loop. Typical pairing: @c this_then_parent (wire continuation)
	/// followed by @c run_this (jump there).
	template<class Promise, class ParentPromise>
	struct run_this
	{
		constexpr static NAGISA_CONCURRENCY_WD4100_INSTANTIATION_ICE(::std::coroutine_handle<Promise>, decltype(auto)) await_suspend(::std::coroutine_handle<Promise> self, ::std::coroutine_handle<ParentPromise> parent) noexcept { return self; }
	};

	/// @brief Awaitable trait: symmetric transfer back to the parent.
	///
	/// Useful when the child has already produced its result during
	/// @c await_suspend (e.g. a synchronously-completing operation) and
	/// just needs to hand control back without going through the
	/// scheduler.
	template<class Promise, class ParentPromise>
	struct run_parent
	{
		constexpr static NAGISA_CONCURRENCY_WD4100_INSTANTIATION_ICE(::std::coroutine_handle<ParentPromise>, decltype(auto)) await_suspend(::std::coroutine_handle<Promise> self, ::std::coroutine_handle<ParentPromise> parent) noexcept { return parent; }
	};

	/// @brief Awaitable trait: register @p parent as the child's continuation.
	///
	/// On suspend, calls @c set_continuation on the child's promise so
	/// that when the child reaches @c final_suspend (using
	/// @c jump_to_continuation), it resumes back into the parent.
	///
	/// Requires the child promise to support @ref set_continuation_t for
	/// the parent handle type.
	template<class Promise, class ParentPromise>
		requires ::std::invocable<set_continuation_t, Promise&, ::std::coroutine_handle<ParentPromise>>
	struct this_then_parent
	{
		using promise_type = Promise;
		using handle_type = ::std::coroutine_handle<promise_type>;

		constexpr static decltype(auto) await_suspend(handle_type self, ::std::coroutine_handle<ParentPromise> parent) noexcept
		{
			set_continuation(self.promise(), parent);
		}
	};

	/// @brief Awaitable trait: splice the child *before* the parent's
	///        existing continuation.
	///
	/// Reads the parent's current continuation, makes the child's
	/// continuation point to it, then sets the parent's continuation to
	/// the child. Used for "I'm joining a chain — when you continue,
	/// continue through me first" semantics.
	///
	/// Requires both promises to expose @ref set_continuation_t and the
	/// parent to also expose @ref continuation_t.
	template<class Promise, class ParentPromise>
		requires
			::std::invocable<set_continuation_t, Promise&, ::std::coroutine_handle<ParentPromise>>
			&& ::std::invocable<set_continuation_t, ParentPromise&, ::std::coroutine_handle<Promise>>
			&& requires(ParentPromise& parent)
			{
				continuation(parent);
				requires ::std::constructible_from<::std::coroutine_handle<>, decltype(continuation(parent))>;
			}
	struct parent_then_this
	{
		using promise_type = Promise;
		using handle_type = ::std::coroutine_handle<promise_type>;

		constexpr static decltype(auto) await_suspend(handle_type self, ::std::coroutine_handle<ParentPromise> parent) noexcept
		{
			set_continuation(self.promise(), continuation(parent));
			set_continuation(parent, self);
		}
	};
}


NAGISA_BUILD_LIB_DETAIL_END

#include "../macro_pop.h"