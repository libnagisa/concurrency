#pragma once

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

namespace awaitable_traits
{
	template<class Promise, class ParentPromise>
	struct destroy_after_resumed
	{
		using promise_type = Promise;
		using handle_type = ::std::coroutine_handle<promise_type>;
		constexpr static decltype(auto) await_resume(handle_type self) noexcept
		{
			self.destroy();
		}
	};
}

namespace promises
{
	struct default_exit
	{
		constexpr static auto final_suspend() noexcept { return ::std::suspend_always{}; }
	};
}

NAGISA_BUILD_LIB_DETAIL_END