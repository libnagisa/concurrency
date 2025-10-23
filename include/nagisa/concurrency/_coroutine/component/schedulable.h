#pragma once

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

namespace awaitable_traits
{
#if NAGISA_CONCURRENCY_USE_EXECUTION
	template<class Promise, class ParentPromise>
		requires requires(Promise& promise, ParentPromise& parent) { promise.set_scheduler(::stdexec::get_scheduler(::stdexec::get_env(parent))); }
	struct capture_scheduler
	{
		using promise_type = Promise;
		using handle_type = ::std::coroutine_handle<promise_type>;

		constexpr static decltype(auto) await_suspend(handle_type self, ::std::coroutine_handle<ParentPromise> parent) noexcept
		{
			self.promise().set_scheduler(::stdexec::get_scheduler(::stdexec::get_env(parent.promise())));
		}
	};

#endif
}

namespace promises
{
#if NAGISA_CONCURRENCY_USE_EXECUTION
	template<::stdexec::scheduler Scheduler>
#else
	template<class Scheduler>
#endif
	struct schedulable
	{
		using scheduler_type = Scheduler;
		struct env_type
		{
#if NAGISA_CONCURRENCY_USE_EXECUTION
			constexpr auto&& query(::stdexec::get_scheduler_t) const noexcept
			{
				return _self->_scheduler;
			}
#endif
			schedulable const* _self;
		};

		constexpr auto get_env() const noexcept { return env_type{ this }; }

		constexpr void set_scheduler(auto&& sched)
			requires ::std::assignable_from<scheduler_type&, decltype(sched)>
		{
			_scheduler = ::std::forward<decltype(sched)>(sched);
		}

		scheduler_type _scheduler;
	};
}

NAGISA_BUILD_LIB_DETAIL_END