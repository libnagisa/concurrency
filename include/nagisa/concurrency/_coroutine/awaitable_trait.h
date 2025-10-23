#pragma once

#include "./awaiter.h"
#include "./scope_guard.h"
#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

template<template<class, class>class... Traits>
struct awaitable_trait_combiner;

template<>
struct awaitable_trait_combiner<> {};

template<template<class, class>class Trait>
struct awaitable_trait_combiner<Trait>
{
	template<class Promise, class Parent>
	using type = Trait<Promise, Parent>;
};

namespace at_details
{
	template<class Trait>
	struct await_ready_t
	{
		template<class Promise>
		constexpr decltype(auto) operator()(::std::coroutine_handle<Promise> handle) const noexcept
			requires requires{ Trait::await_ready(handle); }
		{
			return Trait::await_ready(handle);
		}
	};

	template<class Trait>
	struct await_suspend_t
	{
		template<class Promise, class Parent>
		constexpr decltype(auto) operator()(::std::coroutine_handle<Promise> handle, ::std::coroutine_handle<Parent> parent) const noexcept
			requires requires{ Trait::await_suspend(handle, parent); }
		{
			return Trait::await_suspend(handle, parent);
		}
	};

	template<class Trait>
	struct await_resume_t
	{
		template<class Promise>
		constexpr decltype(auto) operator()(::std::coroutine_handle<Promise> handle) const noexcept
			requires requires{ Trait::await_resume(handle); }
		{
			return Trait::await_resume(handle);
		}
	};

	template<class F1, class F2, class... Args>
	consteval auto compatible_impl() noexcept
	{
		if constexpr (!::std::invocable<F1, Args...> || !::std::invocable<F2, Args...>)
			return true;
		else 
			return ::std::same_as<void, ::std::invoke_result_t<F1, Args...>>
				|| ::std::same_as<void, ::std::invoke_result_t<F2, Args...>>;
	}

	template<class F1, class F2, class... Args>
	concept compatible = requires{ at_details::compatible_impl<F1, F2, Args...>(); };

	template<class F1, class F2>
	constexpr decltype(auto) compatible_invoke(auto... args) noexcept
		requires compatible<F1, F2, decltype(args)...>
	{
		if constexpr(!::std::invocable<F2, decltype(args)...>)
		{
			return ::std::invoke(F1{}, args...);
		}
		else
		{
			if constexpr (!::std::invocable<F1, decltype(args)...>)
			{
				return ::std::invoke(F2{}, args...);
			}
			else
			{
				if constexpr (::std::same_as<void, ::std::invoke_result_t<F2, decltype(args)...>>)
				{
					auto guard_f2 = scope_guard{ [&] noexcept { ::std::invoke(F2{}, args...); } };
					return ::std::invoke(F1{}, args...);
				}
				else
				{
					::std::invoke(F1{}, args...);
					return ::std::invoke(F2{}, args...);
				}
			}
		}
	}
}

template<template<class, class>class Trait1, template<class, class>class Trait2>
struct awaitable_trait_combiner<Trait1, Trait2>
{
	template<class Promise, class Parent>
	struct type
	{
	private:
		using trait1_type = Trait1<Promise, Parent>;
		using trait1_await_ready = at_details::await_ready_t<trait1_type>;
		using trait1_await_suspend = at_details::await_suspend_t<trait1_type>;
		using trait1_await_resume = at_details::await_resume_t<trait1_type>;

		using trait2_type = Trait2<Promise, Parent>;
		using trait2_await_ready = at_details::await_ready_t<trait2_type>;
		using trait2_await_suspend = at_details::await_suspend_t<trait2_type>;
		using trait2_await_resume = at_details::await_resume_t<trait2_type>;

		using handle_type = ::std::coroutine_handle<Promise>;
		using parent_handle_type = ::std::coroutine_handle<Parent>;
	public:

		static_assert(
			at_details::compatible<trait1_await_ready, trait2_await_ready, handle_type>
			&& at_details::compatible<trait1_await_suspend, trait2_await_suspend, handle_type, parent_handle_type>
			&& at_details::compatible<trait1_await_resume, trait2_await_resume, handle_type>
			);

		constexpr static decltype(auto) await_ready(handle_type handle) noexcept
		 	requires ::std::invocable<trait1_await_ready, handle_type> || ::std::invocable<trait2_await_ready, handle_type>
		{
			return at_details::compatible_invoke<trait1_await_ready, trait2_await_ready>(handle);
		}
		constexpr static decltype(auto) await_suspend(handle_type handle, parent_handle_type parent) noexcept
			requires ::std::invocable<trait1_await_suspend, handle_type, parent_handle_type> || ::std::invocable<trait2_await_suspend, handle_type, parent_handle_type>
		{
			return at_details::compatible_invoke<trait1_await_suspend, trait2_await_suspend>(handle, parent);
		}
		constexpr static decltype(auto) await_resume(handle_type handle) noexcept
			requires ::std::invocable<trait1_await_resume, handle_type> || ::std::invocable<trait2_await_resume, handle_type>
		{
			return at_details::compatible_invoke<trait1_await_resume, trait2_await_resume>(handle);
		}
	};
};

template<template<class, class>class Trait1, template<class, class>class Trait2, template<class, class>class... Rest>
struct awaitable_trait_combiner<Trait1, Trait2, Rest...>
	: awaitable_trait_combiner<awaitable_trait_combiner<Trait1, Trait2>::template type, Rest...>
{};

template<class Promise, class Parent, template<class, class>class... Traits>
using awaitable_trait_combiner_t = awaitable_trait_combiner<Traits...>::template type<Promise, Parent>;

template<template<class, class>class Trait>
struct awaitable_trait_instance
{
	template<class Promise, class Parent>
	struct type
	{
	private:
		using trait_type = Trait<Promise, Parent>;
		using trait_await_ready = at_details::await_ready_t<trait_type>;
		using trait_await_suspend = at_details::await_suspend_t<trait_type>;
		using trait_await_resume = at_details::await_resume_t<trait_type>;
		using handle_type = ::std::coroutine_handle<Promise>;
		using parent_handle_type = ::std::coroutine_handle<Parent>;
	public:
		constexpr decltype(auto) await_ready() const noexcept
			requires ::std::invocable<trait_await_ready, handle_type>
		{
			return ::std::invoke(trait_await_ready{}, _coroutine);
		}
		constexpr decltype(auto) await_suspend(parent_handle_type parent) noexcept
			requires ::std::invocable<trait_await_suspend, handle_type, parent_handle_type>
		{
			return ::std::invoke(trait_await_suspend{}, _coroutine, parent);
		}
		constexpr decltype(auto) await_resume() const noexcept
			requires ::std::invocable<trait_await_resume, handle_type>
		{
			return ::std::invoke(trait_await_resume{}, _coroutine);
		}
		handle_type _coroutine;
	};
};

template<class Promise, class Parent, template<class, class>class Trait>
using awaitable_trait_instance_t = awaitable_trait_instance<Trait>::template type<Promise, Parent>;

NAGISA_BUILD_LIB_DETAIL_END