#pragma once

/// @file continuation.h
/// @brief Continuation plumbing — how a coroutine knows what to resume
///        next when it finishes.

#include "../coroutine_handle.h"
#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

/// @brief CPO: sets the "continuation" that a coroutine should resume
///        after @c final_suspend.
///
/// Dispatches in this order:
///   1. @c promise.set_continuation(parent) — member function.
///   2. <tt>tag_invoke(set_continuation_t{}, promise, parent)</tt> — ADL.
///
/// Used by @c awaitable_traits::this_then_parent in @c await_suspend to
/// register the caller as the continuation of the awaited coroutine.
inline constexpr struct set_continuation_t
{
	enum class _requires_result
	{
		member,
		tag_invoke,
		none,
	};
	template<class Promise, class ParentPromise>
	consteval static auto _check() noexcept
	{
		if constexpr (requires(Promise & promise, ::std::coroutine_handle<ParentPromise> parent) { promise.set_continuation(parent); })
			return _requires_result::member;
		else if constexpr (requires(Promise & promise, ::std::coroutine_handle<ParentPromise> parent) { tag_invoke(set_continuation_t{}, promise, parent); })
			return _requires_result::tag_invoke;
		return _requires_result::none;
	}
	template<class ParentPromise>
	constexpr decltype(auto) operator()(auto& promise, ::std::coroutine_handle<ParentPromise> parent) const noexcept
		requires (_check<decltype(promise), ParentPromise>() != _requires_result::none)
	{
		constexpr auto result = _check<decltype(promise), ParentPromise>();
		if constexpr (result == _requires_result::member)
			return promise.set_continuation(parent);
		else if constexpr (result == _requires_result::tag_invoke)
			return tag_invoke(set_continuation_t{}, promise, parent);
	}
} set_continuation{};

/// @brief CPO: reads back the continuation previously set on a promise.
///
/// Dispatches in this order:
///   1. @c promise.continuation() — member function.
///   2. <tt>tag_invoke(continuation_t{}, promise)</tt> — ADL.
inline constexpr struct continuation_t
{
	enum class _requires_result
	{
		member,
		tag_invoke,
		none,
	};
	template<class Promise>
	consteval static auto _check() noexcept
	{
		if constexpr (requires(Promise & promise) { promise.continuation(); })
			return _requires_result::member;
		else if constexpr (requires(Promise & promise) { tag_invoke(continuation_t{}, promise); })
			return _requires_result::tag_invoke;
		return _requires_result::none;
	}
	constexpr decltype(auto) operator()(auto& promise) const noexcept
		requires (_check<decltype(promise)>() != _requires_result::none)
	{
		constexpr auto result = _check<decltype(promise)>();
		if constexpr (result == _requires_result::member)
			return promise.continuation();
		else if constexpr (result == _requires_result::tag_invoke)
			return tag_invoke(continuation_t{}, promise);
	}
} continuation{};

namespace promises
{
	/// @brief Promise mixin: @c final_suspend symmetrically transfers to
	///        the stored continuation (default: a noop coroutine).
	///
	/// Implements both @c set_continuation and @c continuation, so the
	/// pair of CPOs above will find them. Combined with
	/// @c awaitable_traits::this_then_parent, this gives the canonical
	/// "child finishes → resume parent" structured-concurrency flow.
	///
	/// @tparam Handle The continuation handle type; defaults to the
	///         erased @c std::coroutine_handle<>.
	template<coroutine_handle Handle = ::std::coroutine_handle<>>
	struct jump_to_continuation
	{
		using handle_type = Handle;

		constexpr jump_to_continuation() = default;
		constexpr auto final_suspend() const noexcept
		{
			struct awaitable
			{
				constexpr static auto await_ready() noexcept { return false; }
				constexpr auto await_suspend(::std::coroutine_handle<>) const noexcept { return continuation; }
				[[noreturn]] static auto await_resume() noexcept { ::std::abort(); }

				handle_type continuation;
			};
			return awaitable{ _continuation };
		}

		constexpr auto set_continuation(coroutine_handle auto continuation) noexcept
			requires ::std::assignable_from<handle_type&, decltype(continuation)>
		{
			_continuation = continuation;
		}

		constexpr auto continuation() const noexcept { return _continuation; }

		handle_type _continuation = ::std::noop_coroutine();
	};
}

NAGISA_BUILD_LIB_DETAIL_END
