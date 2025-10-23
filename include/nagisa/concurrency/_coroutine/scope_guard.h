#pragma once

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

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