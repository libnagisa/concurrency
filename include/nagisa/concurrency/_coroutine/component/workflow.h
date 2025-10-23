#pragma once

#include "./continuation.h"
#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

namespace awaitable_traits
{
	template<class Promise, class ParentPromise>
	struct default_suspend
	{
		constexpr static decltype(auto) await_suspend(::std::coroutine_handle<Promise> self, ::std::coroutine_handle<ParentPromise> parent) noexcept {}
	};

	template<class Promise, class ParentPromise>
	struct run_this
	{
		constexpr static decltype(auto) await_suspend(::std::coroutine_handle<Promise> self, ::std::coroutine_handle<ParentPromise> parent) noexcept { return self; }
	};

	template<class Promise, class ParentPromise>
	struct run_parent
	{
		constexpr static decltype(auto) await_suspend(::std::coroutine_handle<Promise> self, ::std::coroutine_handle<ParentPromise> parent) noexcept { return parent; }
	};

	template<class Promise, class ParentPromise>
		requires ::std::invocable<set_continuation_t, Promise&, ::std::coroutine_handle<ParentPromise>>
	struct this_then_parent
	{
		using promise_type = Promise;
		using handle_type = ::std::coroutine_handle<promise_type>;

		constexpr static decltype(auto) await_suspend(handle_type self, ::std::coroutine_handle<ParentPromise> parent) noexcept
		{
			set_continuation(self.promise(), parent);
		}
	};

	template<class Promise, class ParentPromise>
		requires
			::std::invocable<set_continuation_t, Promise&, ::std::coroutine_handle<ParentPromise>>
			&& ::std::invocable<set_continuation_t, ParentPromise&, ::std::coroutine_handle<Promise>>
			&& requires(ParentPromise& parent)
			{
				continuation(parent);
				requires ::std::constructible_from<::std::coroutine_handle<>, decltype(continuation(parent))>;
			}
	struct parent_then_this
	{
		using promise_type = Promise;
		using handle_type = ::std::coroutine_handle<promise_type>;

		constexpr static decltype(auto) await_suspend(handle_type self, ::std::coroutine_handle<ParentPromise> parent) noexcept
		{
			set_continuation(self.promise(), continuation(parent));
			set_continuation(parent, self);
		}
	};
}

NAGISA_BUILD_LIB_DETAIL_END