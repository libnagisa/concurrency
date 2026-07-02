#pragma once

/// @file scope_guard.h
/// @brief Minimal scope_guard used internally to chain @c void-returning
///        @c await_suspend bodies safely.
///
/// Calls the stored callable (with the stored arguments) at end of
/// scope, unless @ref scope_guard::dismiss has been called first.
/// Non-movable to prevent accidental ownership transfer.

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

/// @brief Invokes @c Fn(Ts...) on destruction unless dismissed.
///
/// @tparam Fn  Must be @c noexcept-invocable with @c Ts....
/// @tparam Ts  Stored argument types.
template <class Fn, class... Ts>
	requires ::std::is_nothrow_invocable_v<Fn, Ts...>
struct scope_guard
{
	[[no_unique_address]] Fn _function;
	[[no_unique_address]] ::std::tuple<Ts...> _args;
	bool _dismissed{ false };

	constexpr explicit(false) scope_guard(Fn fn, Ts... args) noexcept
		: _function(static_cast<Fn&&>(fn))
		, _args(static_cast<Ts&&>(args)...)
	{
	}
	constexpr scope_guard(scope_guard&&) = delete;
	constexpr scope_guard& operator=(scope_guard&&) = delete;

	/// @brief Cancel: do *not* invoke the callable at destruction.
	constexpr auto dismiss() noexcept { _dismissed = true; }
	constexpr ~scope_guard()
	{
		if (!_dismissed)
		{
			::std::apply(_function, static_cast<decltype(_args)>(_args));
		}
	}
};
template <class Fn, class... Ts>
scope_guard(Fn, Ts...) -> scope_guard<Fn, Ts...>;

NAGISA_BUILD_LIB_DETAIL_END
