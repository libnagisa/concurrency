#pragma once

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN


inline constexpr struct set_scheduler_t
{
	enum class _requires_result
	{
		member,
		tag_invoke,
		none,
	};
	template<class Promise, class Token>
	consteval static auto _check() noexcept
	{
		if constexpr (requires(Promise & promise, Token sche) { promise.set_scheduler(::std::forward<decltype(sche)>(sche)); })
			return _requires_result::member;
		else if constexpr (requires(Promise & promise, Token sche) { tag_invoke(set_scheduler_t{}, promise, ::std::forward<decltype(sche)>(sche)); })
			return _requires_result::tag_invoke;
		return _requires_result::none;
	}
	constexpr decltype(auto) operator()(auto& promise, auto&& sche) const noexcept
		requires (_check<decltype(promise)>() != _requires_result::none)
	{
		constexpr auto result = _check<decltype(promise), decltype(sche)>();
		if constexpr (result == _requires_result::member)
			return promise.set_scheduler(::std::forward<decltype(sche)>(sche));
		else if constexpr (result == _requires_result::tag_invoke)
			return tag_invoke(set_scheduler_t{}, promise, ::std::forward<decltype(sche)>(sche));
	}
} set_scheduler{};

namespace awaitable_traits
{
#if NAGISA_CONCURRENCY_USE_EXECUTION
	template<class Promise, class ParentPromise>
	struct capture_scheduler
	{
		using promise_type = Promise;
		using handle_type = ::std::coroutine_handle<promise_type>;

		constexpr static decltype(auto) await_suspend(handle_type self, ::std::coroutine_handle<ParentPromise> parent) noexcept
			requires requires{ set_scheduler(self.promise(), ::stdexec::get_scheduler(::stdexec::get_env(parent.promise()))); }
		{
			set_scheduler(self.promise(), ::stdexec::get_scheduler(::stdexec::get_env(parent.promise())));
		}
	};

#endif
}

namespace promises
{
#if NAGISA_CONCURRENCY_USE_EXECUTION
	template<::stdexec::scheduler Scheduler = ::stdexec::inline_scheduler>
	struct with_scheduler
	{
		using self_type = with_scheduler;
		using scheduler_type = Scheduler;
		struct env_type
		{
			constexpr auto&& query(::stdexec::get_scheduler_t) const noexcept
			{
				return _self->_scheduler;
			}
			self_type const* _self;
		};

		constexpr auto get_env() const noexcept { return env_type{ this }; }

		constexpr void set_scheduler(auto&& sched)
			requires ::std::assignable_from<scheduler_type&, decltype(sched)>
		{
			_scheduler = ::std::forward<decltype(sched)>(sched);
		}

		scheduler_type _scheduler;
	};
#endif
}

NAGISA_BUILD_LIB_DETAIL_END