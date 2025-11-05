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
	template<class Promise, class Sche>
	consteval static auto _check() noexcept
	{
		if constexpr (requires(Promise & promise, Sche sche) { promise.set_scheduler(::std::forward<decltype(sche)>(sche)); })
			return _requires_result::member;
		else if constexpr (requires(Promise & promise, Sche sche) { tag_invoke(set_scheduler_t{}, promise, ::std::forward<decltype(sche)>(sche)); })
			return _requires_result::tag_invoke;
		return _requires_result::none;
	}
	constexpr decltype(auto) operator()(auto& promise, auto&& sche) const noexcept
		requires (_check<decltype(promise), decltype(sche)>() != _requires_result::none)
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
		{
			if constexpr (requires{ set_scheduler(self.promise(), ::stdexec::get_scheduler(::stdexec::get_env(parent.promise()))); })
				set_scheduler(self.promise(), ::stdexec::get_scheduler(::stdexec::get_env(parent.promise())));
		}
	};

#endif
}

namespace promises
{
#if NAGISA_CONCURRENCY_USE_EXECUTION

	// template<::stdexec::scheduler Scheduler = ::stdexec::inline_scheduler>
	template<class Scheduler = ::stdexec::inline_scheduler>
	struct with_scheduler
	{
		using self_type = with_scheduler;
		using scheduler_type = Scheduler;
		struct env_type
		{
			constexpr auto query(::stdexec::get_scheduler_t) const noexcept { return _self->_scheduler; }
			self_type const* _self;
		};

		constexpr explicit(false) with_scheduler()
			noexcept(::std::is_nothrow_default_constructible_v<scheduler_type>)
			requires ::std::default_initializable<scheduler_type>
			= default;

		constexpr explicit(false) with_scheduler(auto&&...)
			noexcept(::std::is_nothrow_default_constructible_v<scheduler_type>)
			requires ::std::default_initializable<scheduler_type>
			: with_scheduler()
		{}

		constexpr explicit(false) with_scheduler(auto const& env, auto&&...)
			noexcept(::std::is_nothrow_constructible_v<scheduler_type, decltype(::stdexec::get_scheduler(env))>)
			requires ::std::constructible_from<scheduler_type, decltype(::stdexec::get_scheduler(env))>
			: _scheduler(::stdexec::get_scheduler(env))
		{}
		

		constexpr auto get_env() const noexcept { return env_type{ this }; }
		constexpr void set_scheduler(auto&& sched)
			requires ::std::assignable_from<scheduler_type&, decltype(sched)>
		{
			_scheduler = ::std::forward<decltype(sched)>(sched);
		}

		scheduler_type _scheduler;
	};

	template<class Scheduler>
	struct with_scheduler<::std::optional<Scheduler>>
	{
		using self_type = with_scheduler;
		using scheduler_type = Scheduler;
		struct env_type
		{
			constexpr auto query(::stdexec::get_scheduler_t) const noexcept { return _self->_scheduler.value(); }
			self_type const* _self;
		};

		constexpr explicit(false) with_scheduler() noexcept = default;

		constexpr explicit(false) with_scheduler(auto&&...) : with_scheduler() {}

		constexpr explicit(false) with_scheduler(auto const& env, auto&&...)
			noexcept(::std::is_nothrow_constructible_v<::std::optional<scheduler_type>, decltype(::stdexec::get_scheduler(env))>)
			requires ::std::constructible_from<::std::optional<scheduler_type>, decltype(::stdexec::get_scheduler(env))>
			: _scheduler(::stdexec::get_scheduler(env))
		{}


		constexpr auto get_env() const noexcept { return env_type{ this }; }
		constexpr void set_scheduler(auto&& sched)
			requires requires(::std::optional<scheduler_type> s){ s.emplace(::std::forward<decltype(sched)>(sched)); }
		{
			_scheduler.emplace(::std::forward<decltype(sched)>(sched));
		}

		::std::optional<scheduler_type> _scheduler = ::std::nullopt;
	};
#endif
}

NAGISA_BUILD_LIB_DETAIL_END