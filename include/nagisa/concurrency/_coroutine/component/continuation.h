#pragma once

#include "../coroutine_handle.h"
#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

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