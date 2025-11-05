#pragma once

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

template<class Fn, class... Args>
	requires ::std::invocable<Fn, Args...>
struct sync_invoke : ::std::suspend_never
{
	constexpr explicit(false) sync_invoke(auto&& function, auto&&... args)
		noexcept(::std::is_nothrow_constructible_v<Fn, decltype(function)> && ::std::is_nothrow_constructible_v<::std::tuple<Args...>, decltype(args)...>)
		requires ::std::constructible_from<Fn, decltype(function)> && ::std::constructible_from<::std::tuple<Args...>, decltype(args)...>
		: _function(::std::forward<decltype(function)>(function))
		, _args(::std::forward<decltype(args)>(args)...)
	{}
	constexpr decltype(auto) await_resume()
		noexcept(::std::is_nothrow_invocable_v<Fn, Args...>)
	{
		return ::std::apply(::std::move(_function), ::std::move(_args));
	}

	[[no_unique_address]] Fn _function;
	[[no_unique_address]] ::std::tuple<Args...> _args;
};

template<class Fn, class... Args>
sync_invoke(Fn&&, Args...) -> sync_invoke<Fn, Args...>;

template<class Fn, class... Args>
sync_invoke(Fn&, Args...) -> sync_invoke<Fn&, Args...>;

NAGISA_BUILD_LIB_DETAIL_END