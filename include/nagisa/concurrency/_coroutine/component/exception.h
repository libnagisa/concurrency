#pragma once

/// @file exception.h
/// @brief Exception-handling components and the
///        @c get_unhandled_exception_ptr CPO.

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

/// @brief CPO: retrieves a stored @c std::exception_ptr from a promise.
///
/// Dispatches in this order:
///   1. @c promise.get_unhandled_exception_ptr() — member function.
///   2. <tt>tag_invoke(get_unhandled_exception_ptr_t{}, promise)</tt> — ADL.
///
/// If neither is available, the call expression is ill-formed.
/// Used by @c awaitable_traits::rethrow_exception after a child coroutine
/// resumes, to propagate any captured exception up to the awaiter.
inline constexpr struct get_unhandled_exception_ptr_t
{
	enum class _requires_result
	{
		member,
		tag_invoke,
		none,
	};
	template<class Promise>
	consteval static auto _check() noexcept
	{
		if constexpr (requires(Promise & promise) { promise.get_unhandled_exception_ptr(); })
			return _requires_result::member;
		else if constexpr (requires(Promise & promise) { tag_invoke(get_unhandled_exception_ptr_t{}, promise); })
			return _requires_result::tag_invoke;
		return _requires_result::none;
	}
	constexpr decltype(auto) operator()(auto& promise) const noexcept
		requires (_check<decltype(promise)>() != _requires_result::none)
	{
		constexpr auto result = _check<decltype(promise)>();
		if constexpr (result == _requires_result::member)
			return promise.get_unhandled_exception_ptr();
		else if constexpr (result == _requires_result::tag_invoke)
			return tag_invoke(get_unhandled_exception_ptr_t{}, promise);
	}
} get_unhandled_exception_ptr{};

namespace awaitable_traits
{
	/// @brief Awaitable trait: on resume, if the awaited promise stored an
	///        exception, rethrow it in the parent.
	///
	/// Pairs with @c promises::exception<true>, which captures
	/// @c std::current_exception() in @c unhandled_exception().
	template<class Promise, class>
	struct rethrow_exception
	{
		using promise_type = Promise;
		using handle_type = ::std::coroutine_handle<promise_type>;

		constexpr static decltype(auto) await_resume(handle_type handle)
		{
			if constexpr(requires(Promise & promise) { { get_unhandled_exception_ptr(promise) } -> ::std::convertible_to<::std::exception_ptr const&>; })
			{
				if (auto exception_ptr = get_unhandled_exception_ptr(handle.promise()))
				{
					auto is_throw = static_cast<bool>(exception_ptr);
					if (is_throw)
						::std::rethrow_exception(exception_ptr);
				}
			}
		}
	};
}

namespace promises
{
	template<bool>
	struct exception;

	/// @brief Promise mixin: capture unhandled exceptions into an
	///        @c std::exception_ptr for later rethrow.
	///
	/// Provides @c unhandled_exception() and exposes
	/// @c get_unhandled_exception_ptr() so the
	/// @c awaitable_traits::rethrow_exception trait can find it.
	template<>
	struct exception<true>
	{
		constexpr exception() = default;

		auto unhandled_exception() noexcept { _exception = ::std::current_exception(); }
		constexpr auto&& get_unhandled_exception_ptr() const noexcept { return _exception; }
		::std::exception_ptr _exception{};
	};
	/// @brief Promise mixin: abort the process on unhandled exception.
	///
	/// Smaller and faster than the catching version. Use only when the
	/// coroutine body is @c noexcept (or you treat any exception as a
	/// program bug).
	template<>
	struct exception<false>
	{
		constexpr exception() = default;

		[[noreturn]] static auto unhandled_exception() noexcept { ::std::abort(); }
	};
}

NAGISA_BUILD_LIB_DETAIL_END
