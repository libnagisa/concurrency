#pragma once

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN


inline constexpr struct release_returned_value_t
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
		if constexpr (requires(Promise & promise) { promise.release_returned_value(); })
			return _requires_result::member;
		else if constexpr (requires(Promise & promise) { tag_invoke(release_returned_value_t{}, promise); })
			return _requires_result::tag_invoke;
		return _requires_result::none;
	}
	constexpr decltype(auto) operator()(auto& promise) const noexcept
		requires (_check<decltype(promise)>() != _requires_result::none)
	{
		constexpr auto result = _check<decltype(promise)>();
		if constexpr (result == _requires_result::member)
			return promise.release_returned_value();
		else if constexpr (result == _requires_result::tag_invoke)
			return tag_invoke(release_returned_value_t{}, promise);
	}
} release_returned_value{};

namespace awaitable_traits
{
	template<class Promise, class>
	struct release_value
	{
		using promise_type = Promise;
		using handle_type = ::std::coroutine_handle<promise_type>;

		constexpr static decltype(auto) await_resume(handle_type handle) noexcept
		{
			if constexpr (requires(Promise & promise) { release_returned_value(promise); })
			{
				return release_returned_value(handle.promise());
			}
		}
	};
}

namespace promises
{

	template<class T>
	struct value;
	template<::std::move_constructible T>
	struct value<T>
	{
		constexpr value() = default;
		constexpr auto return_value(auto&& value) noexcept(::std::is_nothrow_constructible_v<T, decltype(value)>)
		{
			_value.emplace(::std::forward<decltype(value)>(value));
		}
		constexpr auto release_returned_value() noexcept
		{
			auto temp = ::std::move(*_value);
			_value.reset();
			return temp;
		}

		::std::optional<T> _value = ::std::nullopt;
	};
	template<>
	struct value<void>
	{
		constexpr value() = default;
		constexpr static auto return_void() noexcept {}
	};
}

NAGISA_BUILD_LIB_DETAIL_END