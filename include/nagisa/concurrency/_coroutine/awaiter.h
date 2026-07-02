#pragma once

/// @file awaiter.h
/// @brief Defines the @c awaiter concept and related helpers.
///
/// In C++20 coroutines, an *awaiter* is the object actually consulted by
/// the @c co_await machinery: it answers "am I ready?", "what do I do
/// on suspension?", and "what value does @c co_await produce?".
/// This file models that requirement as a C++20 concept so the rest of
/// the library can constrain templates against it.

#include "./coroutine_handle.h"
#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

/// @brief Valid return types for @c awaiter::await_suspend.
///
/// The C++20 standard accepts three possible shapes for
/// @c await_suspend:
///   - @c void  — unconditionally suspend, let the scheduler decide what runs next.
///   - @c bool  — @c true keeps the coroutine suspended; @c false resumes immediately.
///   - a @c std::coroutine_handle<P> — symmetric transfer: resume that handle.
///
/// @tparam T The candidate return type.
template <class T>
concept await_suspend_result = ::std::same_as<T, void> || ::std::same_as<T, bool> || coroutine_handle<T>;

/// @brief Matches any type that satisfies the C++20 awaiter requirements.
///
/// An awaiter exposes the three-step protocol:
///   - @c await_ready() — returns something contextually convertible to @c bool.
///   - @c await_suspend(h) — accepts the suspending coroutine's handle and
///     returns one of @ref await_suspend_result.
///   - @c await_resume() — produces the result of the @c co_await expression.
///
/// @tparam Awaiter The candidate awaiter type.
/// @tparam ParentPromise The promise type of the suspending coroutine
///         (use @c void to leave the parent promise unconstrained).
///
/// @note An *awaitable* (see @ref awaitable in awaitable.h) is a type that
///       can *produce* an awaiter via @c operator @c co_await or
///       @c promise::await_transform. They are different concepts.
template <class Awaiter, class ParentPromise = void>
concept awaiter = requires(Awaiter & a, ::std::coroutine_handle<ParentPromise> h) {
	a.await_ready() ? 1 : 0;
	{ a.await_suspend(h) } -> await_suspend_result;
	a.await_resume();
};

/// @brief Deduces the result type of @c co_await on @p T inside promise @p P.
///
/// Equivalent to <tt>decltype(awaiter::await_resume())</tt>.
template<class T, class P> requires awaiter<T, P>
using awaiter_result_t = decltype(::std::declval<T&>().await_resume());

NAGISA_BUILD_LIB_DETAIL_END
