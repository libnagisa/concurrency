#pragma once

#include "./awaitable.h"
#include "./awaitable_trait.h"
#include "./component/return_object.h"
#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

template<class Promise, template<class, class>class Awaitable>
struct basic_task
{
private:
	using self_type = basic_task;
public:
	using promise_type = Promise;
	using handle_type = ::std::coroutine_handle<promise_type>;
	template<class ParentPromise = void>
	using awaitable_type = Awaitable<promise_type, ParentPromise>;

	constexpr basic_task(self_type const&) noexcept = delete;
	constexpr self_type& operator=(self_type const&) noexcept = delete;
	constexpr basic_task(self_type&& other) noexcept : _coroutine(::std::exchange(other._coroutine, nullptr)) {}
	constexpr self_type& operator=(self_type&&) noexcept = delete;
	constexpr ~basic_task() noexcept
	{
		if (_coroutine)
			_coroutine.destroy();
	}

	constexpr auto operator co_await() && noexcept
		requires ::std::constructible_from<awaitable_type<void>, handle_type> && awaitable<awaitable_type<void>>
	{
		return awaitable_type<void>{::std::exchange(_coroutine, nullptr)};
	}

	template<class ParentPromise>
		requires ::std::constructible_from<awaitable_type<ParentPromise>, handle_type> && awaitable<awaitable_type<ParentPromise>>
	constexpr auto as_awaitable(ParentPromise& promise) && noexcept
	{
		using awaitable_type = awaitable_type<ParentPromise>;
		if constexpr(::std::constructible_from<awaitable_type, handle_type, ParentPromise&>)
			return awaitable_type(::std::exchange(_coroutine, nullptr), promise);
		else
			return awaitable_type(::std::exchange(_coroutine, nullptr));
	}

//private:
	constexpr explicit(false) basic_task(handle_type const coroutine) : _coroutine(coroutine) {}
	handle_type _coroutine;
};

template<class Promise, template<class, class>class... AwaitableTraits>
struct make_basic_task_with_trait
{
	using type = basic_task<
		Promise,
		awaitable_trait_instance<awaitable_trait_combiner<AwaitableTraits...>::template type>::template type
		>;
};

template<class Promise, template<class, class>class AwaitableTrait>
struct make_basic_task_with_trait<Promise, AwaitableTrait>
{
	using type = basic_task<
		Promise,
		awaitable_trait_instance<AwaitableTrait>::template type
	>;
};

template<class Promise, template<class, class>class... AwaitableTraits>
using make_basic_task_with_trait_t = typename make_basic_task_with_trait<Promise, AwaitableTraits...>::type;

NAGISA_BUILD_LIB_DETAIL_END