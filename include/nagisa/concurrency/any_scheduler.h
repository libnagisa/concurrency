#pragma once

#include <memory>

#include <nagisa/concurrency/coroutine.h>
#include <nagisa/concurrency/environment.h>


#if NAGISA_CONCURRENCY_USE_EXECUTION

NAGISA_BUILD_LIB_DETAIL_BEGIN

struct any_scheduler;

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
		constexpr awaitable_wrapper(awaitable_wrapper const&) noexcept = delete;
		constexpr awaitable_wrapper& operator=(awaitable_wrapper const&) noexcept = delete;
		constexpr awaitable_wrapper(awaitable_wrapper&&) noexcept = default;
		constexpr awaitable_wrapper& operator=(awaitable_wrapper&&) noexcept = default;
		constexpr explicit(false) awaitable_wrapper(auto&&... args) noexcept(::std::is_nothrow_constructible_v<::std::unique_ptr<basic_awaitable>, decltype(args)...>)
			requires ::std::constructible_from<::std::unique_ptr<basic_awaitable>, decltype(args)...>
		: _awaitable(::std::forward<decltype(args)>(args)...)
		{}

		[[nodiscard]] constexpr decltype(auto) await_ready() const { return _awaitable->await_ready(); }
		[[nodiscard]] constexpr decltype(auto) await_suspend(::std::coroutine_handle<> parent) const { return _awaitable->await_suspend(parent); }
		constexpr decltype(auto) await_resume() const { return _awaitable->await_resume(); }

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
		constexpr virtual ::std::unique_ptr<basic_scheduler> _clone() const = 0;
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
		[[nodiscard]] constexpr ::std::unique_ptr<basic_scheduler> _clone() const override { return ::std::make_unique<scheduler_eraser>(_scheduler); }

		scheduler_type _scheduler;
	};
	struct scheduler_wrapper
	{
		struct schedule_type : awaitable_wrapper
		{
			[[nodiscard]] constexpr auto&& get_env() const noexcept { return *this; }
			template<class Tag>
			constexpr any_scheduler query(::stdexec::get_completion_scheduler_t<Tag>) const;
			scheduler_wrapper* _self;
		};
		constexpr scheduler_wrapper(scheduler_wrapper const& other)
			: _scheduler(other._scheduler->_clone())
		{}
		constexpr scheduler_wrapper& operator=(scheduler_wrapper const& other)
		{
			if (this == ::std::addressof(other))
				return *this;
			auto self = ::std::move(*this);
			_scheduler = other._scheduler->_clone();
			return *this;
		}
		constexpr scheduler_wrapper(scheduler_wrapper&&) noexcept = default;
		constexpr scheduler_wrapper& operator=(scheduler_wrapper&&) noexcept = default;
		constexpr explicit(false) scheduler_wrapper(auto&&... args) noexcept(::std::is_nothrow_constructible_v<::std::unique_ptr<basic_scheduler>, decltype(args)...>)
			requires ::std::constructible_from<::std::unique_ptr<basic_scheduler>, decltype(args)...>
		: _scheduler(::std::forward<decltype(args)>(args)...)
		{
		}

		[[nodiscard]] constexpr decltype(auto) schedule() { return schedule_type{ _scheduler->schedule(), this }; }
		constexpr decltype(auto) operator==(scheduler_wrapper const& other) const { return _scheduler->operator==(*other._scheduler); }

		::std::unique_ptr<basic_scheduler> _scheduler;
	};
	constexpr decltype(auto) erase_scheduler(::stdexec::scheduler auto&& s)
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

struct query_create_tag{};
any_scheduler query_create(any::scheduler_wrapper const& wrapper);

struct any_scheduler final
{
	using self_type = any_scheduler;
	constexpr any_scheduler(self_type const&) = default;
	constexpr self_type& operator=(self_type const&) = default;
	constexpr any_scheduler(self_type&&) noexcept = default;
	constexpr self_type& operator=(self_type&&) noexcept = default;
	constexpr explicit(false) any_scheduler(auto&& scheduler)
		requires (!::std::same_as<::std::remove_cvref_t<decltype(scheduler)>, self_type>)
			&& ::stdexec::scheduler<decltype(scheduler)>
		: _wrapper(any::erase_scheduler(::std::forward<decltype(scheduler)>(scheduler)))
	{}
	[[nodiscard]] constexpr decltype(auto) schedule() { return _wrapper.schedule(); }
	constexpr bool operator==(self_type const&) const = default;

	any::scheduler_wrapper _wrapper;
private:
	constexpr any_scheduler(query_create_tag, any::scheduler_wrapper const& wrapper)
		: _wrapper(wrapper)
	{}

	friend any_scheduler query_create(any::scheduler_wrapper const& wrapper)
	{
		return any_scheduler{ query_create_tag{}, wrapper };
	}
};

template <class Tag>
constexpr any_scheduler any::scheduler_wrapper::schedule_type::query(stdexec::get_completion_scheduler_t<Tag>) const
{
	return details::query_create(*_self);
}


NAGISA_BUILD_LIB_DETAIL_END

NAGISA_BUILD_LIB_BEGIN

using details::any_scheduler;

NAGISA_BUILD_LIB_END

#endif

#include <nagisa/build_lib/destruct.h>