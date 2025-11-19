#pragma once

#include <memory>
#include <version>

#include <nagisa/concurrency/coroutine.h>
#include <nagisa/concurrency/environment.h>


#if NAGISA_CONCURRENCY_USE_EXECUTION

NAGISA_BUILD_LIB_DETAIL_BEGIN

#pragma push_macro("NAGISA_CONCURRENCY_UNIQUE_PTR_CONSTEXPR")
#if __cpp_lib_constexpr_memory >= 202202L
#	define NAGISA_CONCURRENCY_UNIQUE_PTR_CONSTEXPR constexpr
#else
#	define NAGISA_CONCURRENCY_UNIQUE_PTR_CONSTEXPR
#endif

namespace any
{
	struct basic_awaitable
	{
		constexpr virtual ~basic_awaitable() noexcept = default;
		constexpr virtual bool await_ready() = 0;
		constexpr virtual ::std::coroutine_handle<> await_suspend(::std::coroutine_handle<> parent) = 0;
		constexpr virtual void await_resume() = 0;
	};
	template<awaitable<> T>
	struct awaitable_eraser : basic_awaitable
	{
		using awaitable_type = T;

		constexpr explicit(true) awaitable_eraser(auto&&... args) noexcept(::std::is_nothrow_constructible_v<awaitable_type, decltype(args)...>)
			requires ::std::constructible_from<awaitable_type, decltype(args)...>
		: _awaitable(::std::forward<decltype(args)>(args)...)
		{}
		constexpr bool await_ready() override { return _awaitable.await_ready(); }
		constexpr ::std::coroutine_handle<> await_suspend(::std::coroutine_handle<> parent) override
		{
			using suspend_type = decltype(_awaitable.await_suspend(parent));
			if constexpr (::std::same_as<suspend_type, void>)
			{
				_awaitable.await_suspend(parent);
				return ::std::noop_coroutine();
			}
			else if constexpr(::std::same_as<suspend_type, bool>)
			{
				if (_awaitable.await_suspend(parent))
				{
					return ::std::noop_coroutine();
				}
				return parent;
			}
			else if constexpr(coroutine_handle<suspend_type>)
			{
				return _awaitable.await_suspend(parent);
			}
			else
			{
				static_assert(!::std::same_as<T, T>);
				return ::std::noop_coroutine();
			}
		}
		constexpr void await_resume() override { _awaitable.await_resume(); }

		awaitable_type _awaitable;
	};
	struct awaitable_wrapper
	{
		NAGISA_CONCURRENCY_UNIQUE_PTR_CONSTEXPR awaitable_wrapper(awaitable_wrapper const&) noexcept = delete;
		NAGISA_CONCURRENCY_UNIQUE_PTR_CONSTEXPR awaitable_wrapper& operator=(awaitable_wrapper const&) noexcept = delete;
		NAGISA_CONCURRENCY_UNIQUE_PTR_CONSTEXPR awaitable_wrapper(awaitable_wrapper&&) noexcept = default;
		NAGISA_CONCURRENCY_UNIQUE_PTR_CONSTEXPR awaitable_wrapper& operator=(awaitable_wrapper&&) noexcept = default;
		NAGISA_CONCURRENCY_UNIQUE_PTR_CONSTEXPR explicit(false) awaitable_wrapper(auto&&... args)
			// noexcept(::std::is_nothrow_constructible_v<::std::unique_ptr<basic_awaitable>, decltype(args)...>)
			requires ::std::constructible_from<::std::unique_ptr<basic_awaitable>, decltype(args)...>
		: _awaitable(::std::forward<decltype(args)>(args)...)
		{
		}

		[[nodiscard]] NAGISA_CONCURRENCY_UNIQUE_PTR_CONSTEXPR decltype(auto) await_ready() const { return _awaitable->await_ready(); }
		[[nodiscard]] NAGISA_CONCURRENCY_UNIQUE_PTR_CONSTEXPR decltype(auto) await_suspend(::std::coroutine_handle<> parent) const { return _awaitable->await_suspend(parent); }
		NAGISA_CONCURRENCY_UNIQUE_PTR_CONSTEXPR decltype(auto) await_resume() const { return _awaitable->await_resume(); }

		::std::unique_ptr<basic_awaitable> _awaitable;
	};
	constexpr decltype(auto) erase_awaitable(awaitable<noop_promise> auto&& a)
	{
		if constexpr (::std::convertible_to<decltype(a), awaitable_wrapper const&>)
			return ::std::forward<decltype(a)>(a);
		else if constexpr (::std::derived_from<decltype(a), basic_awaitable>)
		{
			using awaitable_type = ::std::remove_reference_t<decltype(a)>;
			return awaitable_wrapper(::std::make_unique<awaitable_type>(::std::forward<decltype(a)>(a)));
		}
		else
		{
			noop_promise p{};
			using awaitable_type = ::std::remove_reference_t<decltype(p.await_transform(::std::forward<decltype(a)>(a)))>;
			return awaitable_wrapper(::std::make_unique<awaitable_eraser<awaitable_type>>(p.await_transform(::std::forward<decltype(a)>(a))));
		}
	}
	struct basic_scheduler
	{
		constexpr virtual ~basic_scheduler() noexcept = default;
		constexpr virtual awaitable_wrapper schedule() = 0;
		constexpr virtual bool operator==(basic_scheduler const&) const = 0;
	};
	template<::stdexec::scheduler T>
	struct scheduler_eraser : basic_scheduler
	{
		using scheduler_type = T;

		constexpr explicit(true) scheduler_eraser(auto&&... args) noexcept(::std::is_nothrow_constructible_v<scheduler_type, decltype(args)...>)
			requires ::std::constructible_from<scheduler_type, decltype(args)...>
		: _scheduler(::std::forward<decltype(args)>(args)...)
		{
		}
		constexpr awaitable_wrapper schedule() override { return any::erase_awaitable(::stdexec::schedule(_scheduler)); }
		constexpr bool operator==(basic_scheduler const& other) const override
		{
			auto ptr = dynamic_cast<scheduler_eraser const*>(::std::addressof(other));
			if (!ptr)
				return false;
			return _scheduler == ptr->_scheduler;
		}

		scheduler_type _scheduler;
	};

	struct scheduler_wrapper
	{
		NAGISA_CONCURRENCY_UNIQUE_PTR_CONSTEXPR scheduler_wrapper(scheduler_wrapper const& other) = default;
		NAGISA_CONCURRENCY_UNIQUE_PTR_CONSTEXPR scheduler_wrapper& operator=(scheduler_wrapper const& other) = default;
		NAGISA_CONCURRENCY_UNIQUE_PTR_CONSTEXPR scheduler_wrapper(scheduler_wrapper&&) noexcept = default;
		NAGISA_CONCURRENCY_UNIQUE_PTR_CONSTEXPR scheduler_wrapper& operator=(scheduler_wrapper&&) noexcept = default;
		NAGISA_CONCURRENCY_UNIQUE_PTR_CONSTEXPR explicit(false) scheduler_wrapper(auto&&... args)
			// noexcept(::std::is_nothrow_constructible_v<::std::unique_ptr<basic_scheduler>, decltype(args)...>)
			requires ::std::constructible_from<::std::unique_ptr<basic_scheduler>, decltype(args)...>
		: _scheduler(::std::forward<decltype(args)>(args)...)
		{}
		[[nodiscard]] NAGISA_CONCURRENCY_UNIQUE_PTR_CONSTEXPR decltype(auto) schedule() const { return _scheduler->schedule(); }
		NAGISA_CONCURRENCY_UNIQUE_PTR_CONSTEXPR decltype(auto) operator==(scheduler_wrapper const& other) const { return _scheduler->operator==(*other._scheduler); }

		// ::std::unique_ptr<basic_scheduler> _scheduler;
		::std::shared_ptr<basic_scheduler> _scheduler;
	};
	constexpr ::std::convertible_to<scheduler_wrapper const&> decltype(auto) erase_scheduler(::stdexec::scheduler auto&& s)
	{
		if constexpr (::std::convertible_to<decltype(s), scheduler_wrapper const&>)
			return ::std::forward<decltype(s)>(s);
		else if constexpr (::std::derived_from<decltype(s), basic_scheduler>)
		{
			using scheduler_type = ::std::remove_reference_t<decltype(s)>;
			return scheduler_wrapper(::std::make_unique<scheduler_type>(::std::forward<decltype(s)>(s)));
		}
		else
		{
			using scheduler_type = ::std::remove_reference_t<decltype(s)>;
			return scheduler_wrapper(::std::make_unique<scheduler_eraser<scheduler_type>>(::std::forward<decltype(s)>(s)));
		}
	}
}

struct any_scheduler final
{
	using self_type = any_scheduler;
	struct inplace_construct_tag{};

	struct schedule_type : any::awaitable_wrapper
	{
		using base_type = any::awaitable_wrapper;
		NAGISA_CONCURRENCY_UNIQUE_PTR_CONSTEXPR explicit(false) schedule_type(any::scheduler_wrapper wrapper, auto&&... args)
			// noexcept(::std::is_nothrow_constructible_v<base_type, decltype(args)...>)
			requires ::std::constructible_from<base_type, decltype(args)...>
			: base_type(::std::forward<decltype(args)>(args)...)
			, _scheduler(::std::move(wrapper))
		{}
		constexpr auto&& get_env() const noexcept { return *this; }
		template<class Tag> constexpr auto query(::stdexec::get_completion_scheduler_t<Tag>) const { return any_scheduler{ inplace_construct_tag{}, _scheduler }; }
		any::scheduler_wrapper _scheduler;
	};

	NAGISA_CONCURRENCY_UNIQUE_PTR_CONSTEXPR any_scheduler(self_type const&) = default;
	NAGISA_CONCURRENCY_UNIQUE_PTR_CONSTEXPR self_type& operator=(self_type const&) = default;
	NAGISA_CONCURRENCY_UNIQUE_PTR_CONSTEXPR any_scheduler(self_type&&) noexcept = default;
	NAGISA_CONCURRENCY_UNIQUE_PTR_CONSTEXPR self_type& operator=(self_type&&) noexcept = default;
	NAGISA_CONCURRENCY_UNIQUE_PTR_CONSTEXPR explicit(false) any_scheduler(auto&& scheduler)
		requires (!::std::same_as<::std::remove_cvref_t<decltype(scheduler)>, self_type>)
			&& ::stdexec::scheduler<decltype(scheduler)>
		: _wrapper(any::erase_scheduler(::std::forward<decltype(scheduler)>(scheduler)))
	{}
	NAGISA_CONCURRENCY_UNIQUE_PTR_CONSTEXPR auto schedule() const { return schedule_type(_wrapper, any::erase_awaitable(_wrapper.schedule())); }
	NAGISA_CONCURRENCY_UNIQUE_PTR_CONSTEXPR bool operator==(self_type const&) const = default;

private:
	NAGISA_CONCURRENCY_UNIQUE_PTR_CONSTEXPR any_scheduler(inplace_construct_tag, any::scheduler_wrapper wrapper)
		: _wrapper(::std::move(wrapper))
	{}
	any::scheduler_wrapper _wrapper;
};

#pragma pop_macro("NAGISA_CONCURRENCY_UNIQUE_PTR_CONSTEXPR")

NAGISA_BUILD_LIB_DETAIL_END

NAGISA_BUILD_LIB_BEGIN

using details::any_scheduler;

NAGISA_BUILD_LIB_END

#endif

#include <nagisa/build_lib/destruct.h>
