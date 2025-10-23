#pragma once

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

namespace awaitable_traits
{
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