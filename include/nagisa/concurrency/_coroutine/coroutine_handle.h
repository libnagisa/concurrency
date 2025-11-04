#pragma once

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

namespace coroutine_detail
{
	template <class, template <class...> class>
	inline constexpr bool is_instance_of = false;
	template <class... A, template <class...> class T>
	inline constexpr bool is_instance_of<T<A...>, T> = true;

	template <class A, template <class...> class T>
	concept instance_of = is_instance_of<A, T>;

	template<class Promise>
	constexpr void is_derived_from_std_coroutine_handle(::std::coroutine_handle<Promise> const&);
}
template<class T>
concept coroutine_handle = requires(T t) { coroutine_detail::is_derived_from_std_coroutine_handle(t); };

template<coroutine_handle To>
constexpr decltype(auto) coroutine_handle_cast(coroutine_handle auto&& from) noexcept
{
	using to_type = To;
	return to_type::from_address(::std::forward<decltype(from)>(from).address());
}

NAGISA_BUILD_LIB_DETAIL_END