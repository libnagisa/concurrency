#pragma once

/// @file current_handle.h
/// @brief @ref current_handle — produce the suspending coroutine's own
///        @c std::coroutine_handle as the @c co_await result.

#include "./forward.h"
#include "./from_handle.h"
#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

/// @internal Implementation detail. See @ref current_handle for the
/// user-facing factory.
///
/// Inherits @ref from_handle to do the "construct from parent" trick
/// when used through generic @c get_awaiter, and provides
/// @c as_awaitable so promises that route through it can short-circuit
/// to a @ref forward awaitable typed to the caller's promise.
template<class Handle>
struct current_handle_impl : from_handle<Handle>
{
	using base_type = from_handle<Handle>;

	using base_type::base_type;

	constexpr auto as_awaitable(auto&& parent) const noexcept
	{
		using promise_type = ::std::remove_cvref_t<decltype(parent)>;

		return forward<::std::coroutine_handle<promise_type>>(
			::std::coroutine_handle<promise_type>::from_promise(parent)
		);
	}
};

/// @brief Returns an awaitable whose @c await_resume produces the
///        suspending coroutine's @c std::coroutine_handle.
///
/// Inside a coroutine of promise type @c P, the awaiter yields a
/// @c std::coroutine_handle<P> (via @c as_awaitable). Without the
/// @c as_awaitable hook — e.g. when consumed by generic stdexec
/// machinery — it falls back to whatever @c Handle the user requested
/// (default: erased @c std::coroutine_handle<>).
///
/// @code
///   // Inside a simple_task<void>:
///   std::coroutine_handle<simple_promise<void, ...>> me = co_await current_handle();
/// @endcode
///
/// Used by @c spawn to capture its own handle so it can access its promise.
template<class Handle = ::std::coroutine_handle<>>
constexpr auto current_handle() noexcept
{
	return current_handle_impl<Handle>{};
}

NAGISA_BUILD_LIB_DETAIL_END
