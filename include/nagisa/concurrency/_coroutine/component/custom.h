#pragma once

/// @file custom.h
/// @brief Lambda-driven awaitable traits — quick hooks for one-off
///        @c await_ready / @c await_suspend / @c await_resume behaviors
///        without writing a full trait struct.

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

namespace awaitable_traits
{
	/// @brief Awaitable trait: @c await_ready forwards to a callable @p Ready.
	///
	/// @tparam Ready An NTTP callable invocable as @c Ready(handle).
	///
	/// Example:
	/// @code
	/// constexpr auto always_ready = [](auto) noexcept { return true; };
	/// using my_awaitable = build_awaitable_t<P, Pa,
	///     /* on_ready<P, Pa, always_ready> ... */>;
	/// @endcode
	template<class Promise, class, auto Ready>
	struct on_ready
	{
		using promise_type = Promise;
		using handle_type = ::std::coroutine_handle<promise_type>;

		constexpr static decltype(auto) await_ready(handle_type handle) noexcept
			requires ::std::invocable<decltype(Ready), handle_type>
		{
			return ::std::invoke(Ready, handle);
		}
	};

	/// @brief Awaitable trait: @c await_suspend forwards to a callable @p Suspend.
	///
	/// @tparam Suspend An NTTP callable invocable as @c Suspend(handle, parent).
	///                 Its return type must satisfy @ref await_suspend_result.
	template<class Promise, class Parent, auto Suspend>
	struct on_suspend
	{
		using promise_type = Promise;
		using handle_type = ::std::coroutine_handle<promise_type>;

		constexpr static decltype(auto) await_suspend(handle_type handle, auto&& parent) noexcept
			requires ::std::invocable<decltype(Suspend), handle_type, decltype(parent)>
		{
			return ::std::invoke(Suspend, handle, ::std::forward<decltype(parent)>(parent));
		}
	};

	/// @brief Awaitable trait: @c await_resume forwards to a callable @p Resume.
	///
	/// @tparam Resume An NTTP callable invocable as @c Resume(handle); its
	///                return value becomes the @c co_await result.
	template<class Promise, class Parent, auto Resume>
	struct on_resume
	{
		using promise_type = Promise;
		using handle_type = ::std::coroutine_handle<promise_type>;

		constexpr static decltype(auto) await_resume(handle_type handle) noexcept
			requires ::std::invocable<decltype(Resume), handle_type>
		{
			return ::std::invoke(Resume, handle);
		}
	};
}


NAGISA_BUILD_LIB_DETAIL_END
