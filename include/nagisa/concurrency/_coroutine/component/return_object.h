#pragma once

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

namespace promises
{
	template<class... Ts>
		requires (sizeof...(Ts) > 0) && (sizeof...(Ts) <= 2)
	struct return_object_from_handle;

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