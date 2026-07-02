#pragma once

/// @file from_handle.h
/// @brief @ref from_handle — an awaitable that builds its result from
///        the suspending coroutine's handle.

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

/// @brief Awaitable wrapper: constructs a @c Result from the parent
///        handle inside @c await_suspend, then returns it from
///        @c await_resume.
///
/// @c await_suspend returns @c false (no real suspension — resume
/// immediately), so the coroutine never actually unwinds. The
/// @c Result lives in a union so it doesn't need to be
/// default-constructible.
///
/// Typical use is in @c as_awaitable hooks that need to capture
/// something derived from the caller's promise/handle.
///
/// @tparam Result The constructed value's type; must be move-constructible
///                and destructible from the parent handle.
template<class Result>
	requires ::std::move_constructible<Result>&& ::std::destructible<Result>
struct from_handle : ::std::suspend_always
{
	using result_type = Result;
#if (defined(__clang__) && (__clang_major__ >= 17)) \
	|| (defined(__GNUC__) && (__GNUC__ >= 13))		\
	|| (defined(_MSC_VER ) && (_MSC_VER >= 1929))	\
	//
	constexpr explicit(false) from_handle() noexcept = default;
#else
	constexpr explicit(false) from_handle() noexcept {}
#endif

	constexpr auto await_suspend(auto&& parent)
		noexcept(::std::is_nothrow_constructible_v<result_type, decltype(parent)>)
		requires ::std::constructible_from<result_type, decltype(parent)>
	{
		new(::std::addressof(_result)) result_type(::std::forward<decltype(parent)>(parent));
		return false;
	}
	constexpr auto await_resume() noexcept
	{
		auto result = ::std::move(_result);
		_result.~result_type();
		return result;
	}

	union
	{
		::std::byte _empty{};
		result_type _result;
	};
};



NAGISA_BUILD_LIB_DETAIL_END
