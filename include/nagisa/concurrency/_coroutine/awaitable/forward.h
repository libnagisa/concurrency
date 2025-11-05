#pragma once

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

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