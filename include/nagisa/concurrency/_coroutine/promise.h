#pragma once

/// @file promise.h
/// @brief Defines the @c promise concept — the minimum surface a coroutine
///        promise type must expose to be usable by C++20 coroutines.

#include "./awaitable.h"
#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

/// @brief Matches any C++20 coroutine promise type.
///
/// A promise must:
///   - be a value type (not a reference),
///   - provide @c initial_suspend() and @c final_suspend() that return awaitables,
///   - provide @c get_return_object(),
///   - provide @c unhandled_exception().
///
/// Note that @c return_value / @c return_void are *not* required by this
/// concept — whether a promise needs one or the other depends on the
/// coroutine body and is checked at instantiation time by the compiler.
///
/// @tparam T The candidate promise type.
template<class T>
concept promise = ::std::same_as<T, ::std::remove_reference_t<T>>&& requires(T& promise)
{
	{ promise.initial_suspend() } -> awaitable;
	{ promise.final_suspend() } -> awaitable;
	{ promise.get_return_object() };
	{ promise.unhandled_exception() };
	// requires (requires{ promise.return_value()})
};

NAGISA_BUILD_LIB_DETAIL_END
