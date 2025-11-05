#pragma once

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

template<class,class>
struct noop_awaitable;

template<class Promise, template<class, class>class Awaitable = noop_awaitable>
struct basic_fork;


template<class Promise>
struct basic_fork<Promise, noop_awaitable>
{
private:
	using self_type = basic_fork;
public:
	using promise_type = Promise;
	using handle_type = ::std::coroutine_handle<promise_type>;

	constexpr explicit(false) basic_fork(handle_type const coroutine) noexcept : _coroutine(coroutine) {}

	constexpr auto handle() const noexcept { return _coroutine; }

	handle_type _coroutine;
};

template<class Promise, template<class, class>class Awaitable>
struct basic_fork : basic_fork<Promise, noop_awaitable>
{
private:
	using self_type = basic_fork;
	using base_type = basic_fork<Promise, noop_awaitable>;
public:
	using promise_type = typename base_type::promise_type;
	using handle_type = typename base_type::handle_type;
	template<class ParentPromise = void>
	using awaitable_type = Awaitable<promise_type, ParentPromise>;

	using base_type::base_type;

	constexpr auto operator co_await() noexcept
		requires ::std::constructible_from<awaitable_type<void>, handle_type>
	{
		return awaitable_type<void>{base_type::handle()};
	}

	template<class ParentPromise>
		requires ::std::constructible_from<awaitable_type<ParentPromise>, handle_type>
		|| ::std::constructible_from<awaitable_type<ParentPromise>, handle_type, ParentPromise&>
	constexpr auto as_awaitable(ParentPromise& promise) noexcept
	{
		using awaitable_type = awaitable_type<ParentPromise>;
		if constexpr (::std::constructible_from<awaitable_type, handle_type, ParentPromise&>)
			return awaitable_type(base_type::handle(), promise);
		else
			return awaitable_type(base_type::handle());
	}
};

template<class Promise, template<class, class>class Awaitable = noop_awaitable>
struct basic_task;

template<class Promise>
struct basic_task<Promise, noop_awaitable> : basic_fork<Promise, noop_awaitable>
{
private:
	using self_type = basic_task;
	using base_type = basic_fork<Promise, noop_awaitable>;
public:
	using base_type::base_type;
	constexpr basic_task(self_type const&) noexcept = delete;
	constexpr self_type& operator=(self_type const&) noexcept = delete;
	constexpr basic_task(self_type&& other) noexcept : base_type(other.release()) {}
	constexpr self_type& operator=(self_type&&) noexcept = delete;
	constexpr ~basic_task() noexcept
	{
		if (base_type::handle())
			base_type::handle().destroy();
	}
	constexpr auto release() noexcept { return ::std::exchange(base_type::_coroutine, nullptr); }
};

template<class Promise, template<class, class>class Awaitable>
struct basic_task : basic_task<Promise, noop_awaitable>
{
private:
	using self_type = basic_task;
	using base_type = basic_task<Promise, noop_awaitable>;
public:
	using promise_type = typename base_type::promise_type;
	using handle_type = typename base_type::handle_type;
	template<class ParentPromise = void>
	using awaitable_type = Awaitable<promise_type, ParentPromise>;

	using base_type::base_type;

	constexpr auto operator co_await() && noexcept
		requires ::std::constructible_from<awaitable_type<void>, handle_type>
	{
		return awaitable_type<void>(base_type::release());
	}

	template<class ParentPromise>
		requires ::std::constructible_from<awaitable_type<ParentPromise>, handle_type>
		|| ::std::constructible_from<awaitable_type<ParentPromise>, handle_type, ParentPromise&>
	constexpr auto as_awaitable(ParentPromise& promise) && noexcept
	{
		using awaitable_type = awaitable_type<ParentPromise>;
		if constexpr(::std::constructible_from<awaitable_type, handle_type, ParentPromise&>)
			return awaitable_type(base_type::release(), promise);
		else
			return awaitable_type(base_type::release());
	}
};

NAGISA_BUILD_LIB_DETAIL_END