#pragma once

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

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
					::std::rethrow_exception(exception_ptr);
			}
		}
	};
}

namespace promises
{
	template<bool>
	struct exception;
	template<>
	struct exception<true>
	{
		constexpr exception() = default;

		auto unhandled_exception() noexcept { _exception = ::std::current_exception(); }
		constexpr auto&& get_unhandled_exception_ptr() const noexcept { return _exception; }
		::std::exception_ptr _exception{};
	};
	template<>
	struct exception<false>
	{
		constexpr exception() = default;

		static [[nodiscard]] auto unhandled_exception() noexcept { ::std::abort(); }
	};
}

NAGISA_BUILD_LIB_DETAIL_END