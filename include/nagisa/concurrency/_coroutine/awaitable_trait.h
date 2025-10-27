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
	concept invocable = [] {
		if constexpr (ignore_context<Context>)
			return ::std::invocable<Callable, Args...>;
		else
			return ::std::invocable<Callable, Context, Args...>;
		}();

	template<class Callable, class Context, class... Args>
	struct invoke_result
	{
		static_assert(::std::invocable<Callable, Context, Args...>);
		using type = ::std::invoke_result_t<Callable, Context, Args...>;
	};
	template<class Callable, ignore_context Context, class... Args>
	struct invoke_result<Callable, Context, Args...>
	{
		static_assert(::std::invocable<Callable, Args...>);
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
				|| ::std::same_as<void, invoke_result_t<F2, C2, Args...>>;
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
		if constexpr (!::std::invocable<F2, handle_type, Parent&...>)
		{
			if constexpr (!::std::invocable<F1, handle_type, Parent&...>)
			{
				return compatible_requires_context_result::none;
			}
			else
			{
				return compatible_requires_context_result::left;
			}
		}
		else if constexpr (!::std::invocable<F1, handle_type, Parent&...>)
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
	constexpr decltype(auto) context_left(auto&& context) noexcept
	{
		if constexpr (Result == compatible_requires_context_result::both)
			return ::std::get<0>(::std::forward<decltype(context)>(context));
		else if constexpr (Result == compatible_requires_context_result::left)
			return ::std::forward<decltype(context)>(context);
		else
			return ignore{};
	}

	template<compatible_requires_context_result Result>
	constexpr decltype(auto) context_right(auto&& context) noexcept
	{
		if constexpr (Result == compatible_requires_context_result::both)
			return ::std::get<1>(::std::forward<decltype(context)>(context));
		else if constexpr (Result == compatible_requires_context_result::right)
			return ::std::forward<decltype(context)>(context);
		else
			return ignore{};
	}

	template<class F1, class F2, class Handle, class Parent>
	concept requires_context = at_details::requires_context_impl<F1, F2, Handle, Parent>() != compatible_requires_context_result::none;

	template<class F1, class F2, compatible_requires_context_result Result>
	constexpr decltype(auto) compatible_invoke_with_context_impl(auto&& context, auto&&... args) noexcept
		requires requires
	{
		at_details::compatible_invoke<F1, F2>(
			at_details::context_left<Result>(::std::forward<decltype(context)>(context))
			, at_details::context_right<Result>(::std::forward<decltype(context)>(context))
			, ::std::forward<decltype(args)>(args)...
		);
	}
	{
		return at_details::compatible_invoke<F1, F2>(
			at_details::context_left<Result>(::std::forward<decltype(context)>(context))
			, at_details::context_right<Result>(::std::forward<decltype(context)>(context))
			, ::std::forward<decltype(args)>(args)...
		);
	}

	template<class T1, class T2, template<class>class F, class Promise, class Parent>
	constexpr decltype(auto) compatible_invoke_with_context(auto&& context, auto&&... args) noexcept
		requires requires
	{
		at_details::compatible_invoke_with_context_impl<
			F<T1>, F<T2>
			, at_details::requires_context_impl<create_context_t<T1>, create_context_t<T2>, ::std::coroutine_handle<Promise>, Parent>()>(
			::std::forward<decltype(context)>(context)
			, ::std::forward<decltype(args)>(args)...
		);
	}
	{
		constexpr compatible_requires_context_result result = at_details::requires_context_impl<create_context_t<T1>, create_context_t<T2>, ::std::coroutine_handle<Promise>, Parent>();
		return at_details::compatible_invoke_with_context_impl<F<T1>, F<T2>, result>(::std::forward<decltype(context)>(context), ::std::forward<decltype(args)>(args)...);
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

		using trait1_type = Trait1<Promise, Parent>;
		using t1_create_context = at_details::create_context_t<trait1_type>;

		using trait2_type = Trait2<Promise, Parent>;
		using t2_create_context = at_details::create_context_t<trait2_type>;
	public:
		constexpr static decltype(auto) create_context(handle_type handle) noexcept
			requires requires{ at_details::compatible_create_context<t1_create_context, t2_create_context>(handle); }
		{
			return at_details::compatible_create_context<t1_create_context, t2_create_context>(handle);
		}
		constexpr static decltype(auto) create_context(handle_type handle, auto& parent) noexcept
			requires (!::std::same_as<Parent, void>) && requires{ at_details::compatible_create_context<t1_create_context, t2_create_context>(handle, parent); }
		{
			return at_details::compatible_create_context<t1_create_context, t2_create_context>(handle, parent);
		}

		constexpr static decltype(auto) await_ready(handle_type handle) noexcept
			requires requires{ at_details::compatible_invoke_with_context<trait1_type, trait2_type, at_details::await_ready_t, Promise, Parent>(at_details::ignore{}, handle); }
		{
			return at_details::compatible_invoke_with_context<trait1_type, trait2_type, at_details::await_ready_t, Promise, Parent>(at_details::ignore{}, handle);
		}
		constexpr static decltype(auto) await_ready(auto&& context, handle_type handle) noexcept
			requires requires{ at_details::compatible_invoke_with_context<trait1_type, trait2_type, at_details::await_ready_t, Promise, Parent>(::std::forward<decltype(context)>(context), handle); }
		{
			return at_details::compatible_invoke_with_context<trait1_type, trait2_type, at_details::await_ready_t, Promise, Parent>(::std::forward<decltype(context)>(context), handle);
		}
		template<class Parent2>
		constexpr static decltype(auto) await_suspend(handle_type handle, ::std::coroutine_handle<Parent2> parent) noexcept
			requires requires{ at_details::compatible_invoke_with_context<trait1_type, trait2_type, at_details::await_suspend_t, Promise, Parent>(at_details::ignore{}, handle, parent); }
		{
			return at_details::compatible_invoke_with_context<trait1_type, trait2_type, at_details::await_suspend_t, Promise, Parent>(at_details::ignore{}, handle, parent);
		}
		template<class Parent2>
		constexpr static decltype(auto) await_suspend(auto&& context, handle_type handle, ::std::coroutine_handle<Parent2> parent) noexcept
			requires requires{ at_details::compatible_invoke_with_context<trait1_type, trait2_type, at_details::await_suspend_t, Promise, Parent>(::std::forward<decltype(context)>(context), handle, parent); }
		{
			return at_details::compatible_invoke_with_context<trait1_type, trait2_type, at_details::await_suspend_t, Promise, Parent>(::std::forward<decltype(context)>(context), handle, parent);
		}
		constexpr static decltype(auto) await_resume(handle_type handle) noexcept
			requires requires{ at_details::compatible_invoke_with_context<trait1_type, trait2_type, at_details::await_resume_t, Promise, Parent>(at_details::ignore{}, handle); }
		{
			return at_details::compatible_invoke_with_context<trait1_type, trait2_type, at_details::await_resume_t, Promise, Parent>(at_details::ignore{}, handle);
		}
		constexpr static decltype(auto) await_resume(auto&& context, handle_type handle) noexcept
			requires requires{ at_details::compatible_invoke_with_context<trait1_type, trait2_type, at_details::await_resume_t, Promise, Parent>(::std::forward<decltype(context)>(context), handle); }
		{
			return at_details::compatible_invoke_with_context<trait1_type, trait2_type, at_details::await_resume_t, Promise, Parent>(::std::forward<decltype(context)>(context), handle);
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
	requires (!::std::same_as<Parent, void>) && ::std::invocable<at_details::create_context_t<Trait>, ::std::coroutine_handle<Promise>, Parent&>
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
	requires requires{ typename awaitable_trait_context_t<Trait<Promise, Parent>, Promise, Parent>; }
struct awaitable_trait_instance_t<Trait, Promise, Parent>
{
private:
	using self_type = awaitable_trait_instance_t;
	using trait_type = Trait<Promise, Parent>;
	using trait_await_ready = at_details::await_ready_t<trait_type>;
	using trait_await_suspend = at_details::await_suspend_t<trait_type>;
	using trait_await_resume = at_details::await_resume_t<trait_type>;
	using handle_type = ::std::coroutine_handle<Promise>;
	using context_type = awaitable_trait_context_t<Trait<Promise, Parent>, Promise, Parent>;
public:
	constexpr awaitable_trait_instance_t(handle_type handle) noexcept
		: _context(::std::invoke(at_details::create_context_t<trait_type>{}, handle))
		, _coroutine(handle)
	{}
	constexpr awaitable_trait_instance_t(handle_type handle, auto& parent) noexcept
		: _context(::std::invoke(at_details::create_context_t<trait_type>{}, handle, parent))
		, _coroutine(handle)
	{}

public:
	template<class F>
	constexpr static decltype(auto) invoke(auto& self, auto&&... args) noexcept
		requires ::std::invocable<F, decltype((self._context)), decltype(args)...> || ::std::invocable<F, decltype(args)...>
	{
		if constexpr (::std::invocable<F, decltype((self._context)), decltype(args)...>)
			return ::std::invoke(F{}, self._context, ::std::forward<decltype(args)>(args)...);
		else
			return ::std::invoke(F{}, ::std::forward<decltype(args)>(args)...);
	}
public:

	constexpr decltype(auto) await_ready() noexcept
		requires requires{ self_type::template invoke<trait_await_ready>(*this, handle_type{}); }
	{
		return self_type::template invoke<trait_await_ready>(*this, _coroutine);
	}
	constexpr decltype(auto) await_ready() const noexcept
		requires requires{ self_type::template invoke<trait_await_ready>(*this, handle_type{}); }
	{
		return self_type::template invoke<trait_await_ready>(*this, _coroutine);
	}
	template<class ParentPromise>
	constexpr decltype(auto) await_suspend(::std::coroutine_handle<ParentPromise> parent) noexcept
		requires requires{ self_type::template invoke<trait_await_suspend>(*this, handle_type{}, parent); }
	{
		return self_type::template invoke<trait_await_suspend>(*this, _coroutine, parent);
	}
	constexpr decltype(auto) await_resume() noexcept
		requires requires{ self_type::template invoke<trait_await_resume>(*this, handle_type{}); }
	{
		return self_type::template invoke<trait_await_resume>(*this, _coroutine);
	}
	constexpr decltype(auto) await_resume() const noexcept
		requires requires{ self_type::template invoke<trait_await_resume>(*this, handle_type{}); }
	{
		return self_type::template invoke<trait_await_resume>(*this, _coroutine);
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