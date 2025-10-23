#pragma once

#include "./environment.h"


NAGISA_BUILD_LIB_DETAIL_BEGIN

namespace awaitable_traits
{
	template<class Promise, class>
	struct ready_false
	{
		using promise_type = Promise;
		using handle_type = ::std::coroutine_handle<promise_type>;

		constexpr static decltype(auto) await_ready(handle_type handle) noexcept
		{
			return false;
		}
	};
	template<class Promise, class>
	struct ready_true
	{
		using promise_type = Promise;
		using handle_type = ::std::coroutine_handle<promise_type>;

		constexpr static decltype(auto) await_ready(handle_type handle) noexcept
		{
			return true;
		}
	};

	template<class Promise, class>
	struct ready_if_done
	{
		using promise_type = Promise;
		using handle_type = ::std::coroutine_handle<promise_type>;

		constexpr static decltype(auto) await_ready(handle_type handle) noexcept
		{
			return handle.done();
		}
	};
}

namespace promises
{
	struct lazy
	{
		constexpr static auto initial_suspend() noexcept { return ::std::suspend_always{}; }
	};

	struct eager
	{
		constexpr static auto initial_suspend() noexcept { return ::std::suspend_never{}; }
	};
}

NAGISA_BUILD_LIB_DETAIL_END