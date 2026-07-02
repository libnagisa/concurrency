#pragma once

/// @file return_object.h
/// @brief Promise mixin that synthesizes @c get_return_object from the
///        coroutine handle.

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

namespace promises
{
	template<class... Ts>
		requires (sizeof...(Ts) > 0) && (sizeof...(Ts) <= 2)
	struct return_object_from_handle;

	/// @brief Promise mixin: implements @c get_return_object by constructing
	///        @p Task from @c coroutine_handle<Derived>.
	///
	/// CRTP-style: @p Derived must be the final promise type so that
	/// @c std::coroutine_handle can name it.
	///
	/// Use it like:
	/// @code
	///   struct my_task;
	///   struct my_promise : promises::return_object_from_handle<my_promise, my_task> { ... };
	///   struct my_task { my_task(std::coroutine_handle<my_promise>); ... };
	/// @endcode
	///
	/// @tparam Derived The promise type (used for CRTP).
	/// @tparam Task    The return-object type; must be constructible from
	///                 @c std::coroutine_handle<Derived>.
	template<class Derived, class Task>
	struct return_object_from_handle<Derived, Task>
	{
	private:
		using self_type = return_object_from_handle;
		using promise_type = Derived;
		constexpr auto&& as_derived() noexcept { return static_cast<promise_type&>(*this); }
		constexpr auto&& as_derived() const noexcept { return static_cast<promise_type const&>(*this); }
	public:
		constexpr auto get_return_object() noexcept
			requires ::std::constructible_from<Task, ::std::coroutine_handle<Derived>>
		{
			return Task(::std::coroutine_handle<promise_type>::from_promise(as_derived()));
		}
	};

#if defined(__cpp_explicit_this_parameter)
	/// @brief Single-arg variant (C++23 "deducing this") that infers
	///        @c Derived from the implicit object parameter.
	///
	/// Only available when the compiler supports the
	/// @c __cpp_explicit_this_parameter feature.
	template<class Task>
	struct return_object_from_handle<Task>
	{
		constexpr auto get_return_object(this auto& self) noexcept
			requires ::std::constructible_from<Task, ::std::coroutine_handle<::std::remove_cvref_t<decltype(self)>>>
		{
			return Task(::std::coroutine_handle<::std::remove_cvref_t<decltype(self)>>::from_promise(self));
		}
	};
#endif
}


NAGISA_BUILD_LIB_DETAIL_END
