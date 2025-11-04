#pragma once

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

template<class Result>
	requires ::std::move_constructible<Result>&& ::std::destructible<Result>
struct from_handle
{
	using result_type = Result;

	constexpr explicit(false) from_handle() noexcept = default;

	constexpr static auto await_ready() noexcept { return false; }
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
		::std::byte _empty[1]{};
		result_type _result;
	};
};



NAGISA_BUILD_LIB_DETAIL_END