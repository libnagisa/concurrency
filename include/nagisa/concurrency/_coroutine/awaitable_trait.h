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
	struct create_context_t
	{
		template<class Promise>
		constexpr decltype(auto) operator()(::std::coroutine_handle<Promise> handle) const noexcept
			requires requires{ Trait::create_context(handle); }
		{
			return Trait::create_context(handle);
		}
		template<class Promise, class Parent>
		constexpr decltype(auto) operator()(::std::coroutine_handle<Promise> handle, Parent& parent) const noexcept
			requires requires{ Trait::create_context(handle, parent); }
		{
			return Trait::create_context(handle, parent);
		}
		template<class Promise, class Parent>
		constexpr decltype(auto) operator()(::std::coroutine_handle<Promise> handle, Parent& parent) const noexcept
			requires requires{ Trait::create_context(handle); }
		{
			return Trait::create_context(handle);
		}
	};

	template<class Trait>
	struct await_ready_t
	{
		template<class Promise>
		constexpr decltype(auto) operator()(::std::coroutine_handle<Promise> handle) const noexcept
			requires requires{ Trait::await_ready(handle); }
		{
			return Trait::await_ready(handle);
		}
		template<class Promise>
		constexpr decltype(auto) operator()(auto&& context, ::std::coroutine_handle<Promise> handle) const noexcept
			requires requires{ Trait::await_ready(::std::forward<decltype(context)>(context), handle); }
		{
			return Trait::await_ready(::std::forward<decltype(context)>(context), handle);
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
		template<class Promise, class Parent>
		constexpr decltype(auto) operator()(auto&& context, ::std::coroutine_handle<Promise> handle, ::std::coroutine_handle<Parent> parent) const noexcept
			requires requires{ Trait::await_suspend(::std::forward<decltype(context)>(context), handle, parent); }
		{
			return Trait::await_suspend(::std::forward<decltype(context)>(context), handle, parent);
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
		template<class Promise>
		constexpr decltype(auto) operator()(auto&& context, ::std::coroutine_handle<Promise> handle) const noexcept
			requires requires{ Trait::await_resume(::std::forward<decltype(context)>(context), handle); }
		{
			return Trait::await_resume(::std::forward<decltype(context)>(context), handle);
		}
	};

	struct ignore{};
	template<class T>
	concept ignore_context = ::std::same_as<::std::remove_cvref_t<T>, ignore>;

	template<class Callable, class Context, class... Args>
	concept invocable = ignore_context<Context> ? ::std::invocable<Callable, Args...> : ::std::invocable<Callable, Context, Args...>;

	template<class Callable, class Context, class... Args>
	struct invoke_result
	{
		using type = ::std::invoke_result_t<Callable, Context, Args...>;
	};
	template<class Callable, ignore_context Context, class... Args>
	struct invoke_result
	{
		using type = ::std::invoke_result_t<Callable, Args...>;
	};
	template<class Callable, class Context, class... Args>
	using invoke_result_t = typename invoke_result<Callable, Context, Args...>::type;
	constexpr decltype(auto) invoke(auto&& callable, auto&& context, auto&&... args) noexcept
		requires invocable<decltype(callable), decltype(context), decltype(args)...>
	{
		if constexpr (ignore_context<decltype(context)>)
		{
			return ::std::invoke(::std::forward<decltype(callable)>(callable), ::std::forward<decltype(args)>(args)...);
		}
		else
		{
			return ::std::invoke(::std::forward<decltype(callable)>(callable), ::std::forward<decltype(context)>(context), ::std::forward<decltype(args)>(args)...);
		}
	}


	template<class F1, class F2, class C1, class C2, class... Args>
	consteval auto compatible_impl() noexcept
	{
		constexpr auto i1 = invocable<F1, C1, Args...>;
		constexpr auto i2 = invocable<F2, C2, Args...>;
		if constexpr (!i1 && !i2)
			return false;
		else if constexpr (!i1 || !i2)
			return true;
		else
		{
			return ::std::same_as<void, invoke_result_t<F1, C1, Args...>>
				|| ::std::same_as<void, invoke_result_t<F2, C1, Args...>>;
		}
	}

	template<class F1, class F2, class C1, class C2, class... Args>
	concept compatible = requires{ requires at_details::compatible_impl<F1, F2, C1, C2, Args...>(); };

	template<class F1, class F2>
	constexpr decltype(auto) compatible_invoke(auto&& context1, auto&& context2, auto&&... args) noexcept
		requires compatible<F1, F2, decltype(context1), decltype(context2), decltype(args)...>
	{
		if constexpr(!invocable<F2, decltype(context2), decltype(args)...>)
		{
			return at_details::invoke(F1{}, ::std::forward<decltype(context1)>(context1), ::std::forward<decltype(args)>(args)...);
		}
		else
		{
			if constexpr (!invocable<F1, decltype(context1), decltype(args)...>)
			{
				return at_details::invoke(F2{}, ::std::forward<decltype(context2)>(context2), ::std::forward<decltype(args)>(args)...);
			}
			else
			{
				if constexpr (::std::same_as<void, invoke_result_t<F2, decltype(context2), decltype(args)...>>)
				{
					auto guard_f2 = scope_guard{ [&] noexcept { at_details::invoke(F2{}, ::std::forward<decltype(context2)>(context2), ::std::forward<decltype(args)>(args)...); } };
					return at_details::invoke(F1{}, ::std::forward<decltype(context1)>(context1), ::std::forward<decltype(args)>(args)...);
				}
				else
				{
					at_details::invoke(F1{}, ::std::forward<decltype(context1)>(context1), ::std::forward<decltype(args)>(args)...);
					return at_details::invoke(F2{}, ::std::forward<decltype(context2)>(context2), ::std::forward<decltype(args)>(args)...);
				}
			}
		}
	}

	enum class compatible_requires_context_result
	{
		none,
		left,
		right,
		both,
	};

	template<class F1, class F2, class Handle, class... Parent>
	consteval compatible_requires_context_result requires_context_impl_impl() noexcept
	{
		using handle_type = Handle;
		if constexpr (!::std::invocable<F2, handle_type, Parent...>)
		{
			if constexpr (!::std::invocable<F1, handle_type, Parent...>)
			{
				return compatible_requires_context_result::none;
			}
			else
			{
				return compatible_requires_context_result::left;
			}
		}
		else if constexpr (!::std::invocable<F1, handle_type, Parent...>)
		{
			return compatible_requires_context_result::right;
		}
		else
		{
			return compatible_requires_context_result::both;
		}
	}
	template<class F1, class F2>
	constexpr decltype(auto) compatible_create_context(auto handle, auto&&... args) noexcept
		requires (at_details::requires_context_impl_impl<F1, F2, decltype(handle), decltype(args)...>() != compatible_requires_context_result::none)
	{
		constexpr compatible_requires_context_result result = at_details::requires_context_impl_impl<F1, F2, decltype(handle), decltype(args)...>();
		if constexpr (result == compatible_requires_context_result::left)
		{
			return ::std::invoke(F1{}, handle, ::std::forward<decltype(args)>(args)...);
		}
		else if constexpr (result == compatible_requires_context_result::right)
		{
			return ::std::invoke(F2{}, handle, ::std::forward<decltype(args)>(args)...);
		}
		else
		{
			static_assert(result == compatible_requires_context_result::both);
			return ::std::make_tuple(::std::invoke(F1{}, handle, ::std::forward<decltype(args)>(args)...), ::std::invoke(F2{}, handle, ::std::forward<decltype(args)>(args)...));
		}
	}

	template<class F1, class F2, class Handle, class Parent>
	consteval compatible_requires_context_result requires_context_impl() noexcept
	{
		if constexpr (::std::same_as<void, Parent>)
		{
			return at_details::requires_context_impl_impl<F1, F2, Handle>();
		}
		else
		{
			return at_details::requires_context_impl_impl<F1, F2, Handle, Parent>();
		}
	}

	template<compatible_requires_context_result Result>
	constexpr decltype(auto) transform_context(auto&& context) noexcept
	{
		if constexpr (Result == compatible_requires_context_result::both)
		{
			return ::std::forward<decltype(context)>(context);
		}
		else if constexpr (Result == compatible_requires_context_result::left)
		{
			return ::std::make_tuple(::std::forward<decltype(context)>(context), ignore{});
		}
		else if constexpr (Result == compatible_requires_context_result::right)
		{
			return ::std::make_tuple(ignore{}, ::std::forward<decltype(context)>(context));
		}
		else if constexpr (Result == compatible_requires_context_result::none)
		{
			return ::std::make_tuple(ignore{}, ignore{});
		}
	}

	template<class F1, class F2, class Handle, class Parent>
	concept requires_context = at_details::requires_context_impl<F1, F2, Handle, Parent>() != compatible_requires_context_result::none;

	template<class F1, class F2, class Promise, class Parent>
	constexpr decltype(auto) compatible_invoke_with_context(auto&& context, auto&&... args) noexcept
		requires requires
	{
		at_details::compatible_invoke<F1, F2>(
			::std::get<0>(at_details::transform_context<at_details::requires_context_impl<F1, F2, ::std::coroutine_handle<Promise>, Parent>()>(::std::forward<decltype(context)>(context)))
			, ::std::get<1>(at_details::transform_context<at_details::requires_context_impl<F1, F2, ::std::coroutine_handle<Promise>, Parent>()>(::std::forward<decltype(context)>(context)))
			, ::std::forward<decltype(args)>(args)...
		);
	}
	{
		using handle_type = ::std::coroutine_handle<Promise>;
		constexpr compatible_requires_context_result result = at_details::requires_context_impl<F1, F2, handle_type, Parent>();
		auto&& [first, second] = at_details::transform_context<result>(::std::forward<decltype(context)>(context));
		return at_details::compatible_invoke<F1, F2>(::std::forward<decltype(first)>(first), ::std::forward<decltype(second)>(second), ::std::forward<decltype(args)>(args)...);
	}
}

template<template<class, class>class Trait1, template<class, class>class Trait2>
struct awaitable_trait_combiner<Trait1, Trait2>
{
	template<class Promise, class Parent>
	struct type
	{
	private:
		using handle_type = ::std::coroutine_handle<Promise>;
		using parent_handle_type = ::std::coroutine_handle<Parent>;

		using trait1_type = Trait1<Promise, Parent>;
		using t1_await_ready = at_details::await_ready_t<trait1_type>;
		using t1_await_suspend = at_details::await_suspend_t<trait1_type>;
		using t1_await_resume = at_details::await_resume_t<trait1_type>;
		using t1_create_context = at_details::create_context_t<trait1_type>;

		using trait2_type = Trait2<Promise, Parent>;
		using t2_await_ready = at_details::await_ready_t<trait2_type>;
		using t2_await_suspend = at_details::await_suspend_t<trait2_type>;
		using t2_await_resume = at_details::await_resume_t<trait2_type>;
		using t2_create_context = at_details::create_context_t<trait2_type>;
	public:
		constexpr static decltype(auto) create_context(handle_type handle) noexcept
			requires requires{ at_details::compatible_create_context<t1_create_context, t2_create_context>(handle); }
		{
			return at_details::compatible_create_context<t1_create_context, t2_create_context>(handle);
		}
		constexpr static decltype(auto) create_context(handle_type handle, Parent& parent) noexcept
			requires requires{ at_details::compatible_create_context<t1_create_context, t2_create_context>(handle, parent); }
		{
			return at_details::compatible_create_context<t1_create_context, t2_create_context>(handle, parent);
		}

		constexpr static decltype(auto) await_ready(handle_type handle) noexcept
			requires requires{ at_details::compatible_invoke_with_context<t1_await_ready, t2_await_ready>(at_details::ignore{}, handle); }
		{
			return at_details::compatible_invoke_with_context<t1_await_ready, t2_await_ready>(at_details::ignore{}, handle);
		}
		constexpr static decltype(auto) await_ready(auto&& context, handle_type handle) noexcept
			requires requires{ at_details::compatible_invoke_with_context<t1_await_ready, t2_await_ready>(::std::forward<decltype(context)>(context), handle); }
		{
			return at_details::compatible_invoke_with_context<t1_await_ready, t2_await_ready>(::std::forward<decltype(context)>(context), handle);
		}
		constexpr static decltype(auto) await_suspend(handle_type handle, parent_handle_type parent) noexcept
			requires requires{ at_details::compatible_invoke_with_context<t1_await_suspend, t2_await_suspend>(at_details::ignore{}, handle, parent); }
		{
			return at_details::compatible_invoke_with_context<t1_await_suspend, t2_await_suspend>(at_details::ignore{}, handle, parent);
		}
		constexpr static decltype(auto) await_suspend(auto&& context, handle_type handle, parent_handle_type parent) noexcept
			requires requires{ at_details::compatible_invoke_with_context<t1_await_suspend, t2_await_suspend>(::std::forward<decltype(context)>(context), handle, parent); }
		{
			return at_details::compatible_invoke_with_context<t1_await_suspend, t2_await_suspend>(::std::forward<decltype(context)>(context), handle, parent);
		}
		constexpr static decltype(auto) await_resume(handle_type handle) noexcept
			requires requires{ at_details::compatible_invoke_with_context<t1_await_resume, t1_await_resume>(at_details::ignore{}, handle); }
		{
			return at_details::compatible_invoke_with_context<t1_await_resume, t1_await_resume>(at_details::ignore{}, handle);
		}
		constexpr static decltype(auto) await_resume(auto&& context, handle_type handle) noexcept
			requires requires{ at_details::compatible_invoke_with_context<t1_await_resume, t1_await_resume>(::std::forward<decltype(context)>(context), handle); }
		{
			return at_details::compatible_invoke_with_context<t1_await_resume, t1_await_resume>(::std::forward<decltype(context)>(context), handle);
		}
	};
};

template<template<class, class>class Trait1, template<class, class>class Trait2, template<class, class>class... Rest>
struct awaitable_trait_combiner<Trait1, Trait2, Rest...>
	: awaitable_trait_combiner<awaitable_trait_combiner<Trait1, Trait2>::template type, Rest...>
{};

template<class Promise, class Parent, template<class, class>class... Traits>
using awaitable_trait_combiner_t = awaitable_trait_combiner<Traits...>::template type<Promise, Parent>;

template<class Trait, class Promise, class Parent>
struct awaitable_trait_context;


template<class Trait, class Promise, class Parent>
	requires ::std::invocable<at_details::create_context_t<Trait>, ::std::coroutine_handle<Promise>, Parent&>
struct awaitable_trait_context<Trait, Promise, Parent>
{
	using type = ::std::invoke_result_t<at_details::create_context_t<Trait>, ::std::coroutine_handle<Promise>, Parent&>;
};

template<class Trait, class Promise>
	requires ::std::invocable<at_details::create_context_t<Trait>, ::std::coroutine_handle<Promise>>
struct awaitable_trait_context<Trait, Promise, void>
{
	using type = ::std::invoke_result_t<at_details::create_context_t<Trait>, ::std::coroutine_handle<Promise>>;
};

template<class Trait, class Promise, class Parent>
	requires requires{ typename awaitable_trait_context<Trait, Promise, Parent>::type; }
using awaitable_trait_context_t = typename awaitable_trait_context<Trait, Promise, Parent>::type;

template<template<class, class>class Trait, class Promise, class Parent>
struct awaitable_trait_instance_t
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


template<template<class, class>class Trait, class Promise, class Parent>
	requires requires{ typename awaitable_trait_context<Trait<Promise, Parent>, Promise, Parent>; }
struct awaitable_trait_instance_t<Trait, Promise, Parent>
{
private:
	using trait_type = Trait<Promise, Parent>;
	using trait_await_ready = at_details::await_ready_t<trait_type>;
	using trait_await_suspend = at_details::await_suspend_t<trait_type>;
	using trait_await_resume = at_details::await_resume_t<trait_type>;
	using handle_type = ::std::coroutine_handle<Promise>;
	using parent_handle_type = ::std::coroutine_handle<Parent>;
	using context_type = awaitable_trait_context<Trait<Promise, Parent>, Promise, Parent>;
public:
	constexpr awaitable_trait_instance_t(handle_type handle, Parent& parent) noexcept
		: _context(::std::invoke(at_details::create_context_t<trait_type>{}, handle, parent))
		, _coroutine(handle)
	{}

	constexpr decltype(auto) await_ready() noexcept
		requires ::std::invocable<trait_await_ready, context_type&, handle_type>
	{
		return ::std::invoke(trait_await_ready{}, _context, _coroutine);
	}
	constexpr decltype(auto) await_ready() const noexcept
		requires ::std::invocable<trait_await_ready, context_type const&, handle_type>
	{
		return ::std::invoke(trait_await_ready{}, _context, _coroutine);
	}
	constexpr decltype(auto) await_suspend(parent_handle_type parent) noexcept
		requires ::std::invocable<trait_await_suspend, context_type&, handle_type, parent_handle_type>
	{
		return ::std::invoke(trait_await_suspend{}, _context, _coroutine, parent);
	}
	constexpr decltype(auto) await_resume() noexcept
		requires ::std::invocable<trait_await_resume, context_type&, handle_type>
	{
		return ::std::invoke(trait_await_resume{}, _context, _coroutine);
	}
	constexpr decltype(auto) await_resume() const noexcept
		requires ::std::invocable<trait_await_resume, context_type const&, handle_type>
	{
		return ::std::invoke(trait_await_resume{}, _context, _coroutine);
	}
	context_type _context;
	handle_type _coroutine;
};

template<template<class, class>class Trait>
struct awaitable_trait_instance
{
	template<class Promise, class Parent>
	using type = awaitable_trait_instance_t<Trait, Promise, Parent>;
};

NAGISA_BUILD_LIB_DETAIL_END