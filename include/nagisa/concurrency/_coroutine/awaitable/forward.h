#pragma once

/// @file forward.h
/// @brief @ref forward — an awaitable that holds a precomputed value
///        and produces it from @c await_resume.

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

/// @brief Awaitable wrapper that yields a stored value without suspending.
///
/// @c await_ready returns @c true (inherited from @c suspend_never), so
/// @c co_await @c forward{x} is equivalent to @c std::move(x) but goes
/// through the coroutine expression. Useful inside @c as_awaitable
/// hooks that must return an awaitable even when the value is already
/// known.
///
/// @tparam Result The stored type; must be move-constructible and destructible.
///
/// @code
///   int x = co_await forward<int>(42);   // same as: int x = 42;
/// @endcode
template<class Result>
	requires ::std::move_constructible<Result> && ::std::destructible<Result>
struct forward : ::std::suspend_never
{
	using result_type = Result;

	constexpr explicit(false) forward(auto&&... args)
		noexcept(::std::is_nothrow_constructible_v<result_type, decltype(args)...>)
		requires ::std::constructible_from<result_type, decltype(args)...>
		: _result(::std::forward<decltype(args)>(args)...)
	{}

	constexpr auto await_resume() noexcept { return ::std::move(_result); }

	result_type _result;
};
template<class T>
forward(T&) -> forward<T>;
template<class T>
forward(T&&) -> forward<T>;



NAGISA_BUILD_LIB_DETAIL_END
