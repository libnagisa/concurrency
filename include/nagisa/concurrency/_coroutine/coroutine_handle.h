#pragma once

/// @file coroutine_handle.h
/// @brief Concept and cast helper for @c std::coroutine_handle specializations.

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

namespace coroutine_detail
{
	/// @internal Trait: @c true iff @c A is a specialization of template @c T.
	template <class, template <class...> class>
	inline constexpr bool is_instance_of = false;
	template <class... A, template <class...> class T>
	inline constexpr bool is_instance_of<T<A...>, T> = true;

	template <class A, template <class...> class T>
	concept instance_of = is_instance_of<A, T>;

	/// @internal Helper used only inside a @c requires clause to detect
	/// "this type can decay to @c std::coroutine_handle<P> for some @c P".
	/// Not defined because it is never odr-used.
	template<class Promise>
	constexpr void is_derived_from_std_coroutine_handle(::std::coroutine_handle<Promise> const&);
}

/// @brief Matches any @c std::coroutine_handle specialization,
///        including @c std::coroutine_handle<> (the type-erased form).
///
/// User-defined types that merely *resemble* a coroutine handle do not
/// satisfy this concept — it specifically detects the standard template.
template<class T>
concept coroutine_handle = requires(T t) { coroutine_detail::is_derived_from_std_coroutine_handle(t); };

/// @brief Reinterprets a coroutine handle as a different specialization
///        without touching the underlying coroutine state.
///
/// Useful when you have a @c std::coroutine_handle<> and know it actually
/// refers to a coroutine whose promise is @c P, or vice-versa.
///
/// @warning Like @c coroutine_handle::from_address, this is unchecked —
///          if the runtime promise type doesn't match @p To, subsequent
///          access to @c .promise() is undefined behavior.
///
/// @tparam To The target handle specialization.
/// @param from Source handle.
template<coroutine_handle To>
constexpr decltype(auto) coroutine_handle_cast(coroutine_handle auto&& from) noexcept
{
	using to_type = To;
	return to_type::from_address(::std::forward<decltype(from)>(from).address());
}

NAGISA_BUILD_LIB_DETAIL_END
