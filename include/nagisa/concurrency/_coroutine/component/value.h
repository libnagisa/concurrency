#pragma once

/// @file value.h
/// @brief Return-value storage components and the
///        @c release_returned_value CPO.

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN


/// @brief CPO: pulls the stored return value out of a promise (by move).
///
/// Dispatches in this order:
///   1. @c promise.release_returned_value() — member function.
///   2. <tt>tag_invoke(release_returned_value_t{}, promise)</tt> — ADL.
///
/// Used by @c awaitable_traits::release_value during @c await_resume to
/// hand the @c co_return value over to the awaiter.
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
	/// @brief Awaitable trait: on resume, return whatever the promise stored.
	///
	/// Calls @c release_returned_value on the promise. For
	/// @c promises::value<void>, the trait's @c await_resume is itself
	/// void, so the awaiter yields no value.
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

	/// @brief Promise mixin: stores a @c co_return value of type @p T.
	///
	/// Implements @c return_value (move-construct into an internal
	/// @c std::optional) and @c release_returned_value (move out and reset).
	///
	/// @tparam T Must be move-constructible.
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
	/// @brief Promise mixin for coroutines that don't produce a value.
	///
	/// Provides @c return_void; intentionally does **not** define
	/// @c release_returned_value, so @c release_value's @c await_resume
	/// is itself void.
	template<>
	struct value<void>
	{
		constexpr value() = default;
		constexpr static auto return_void() noexcept {}
	};
}

NAGISA_BUILD_LIB_DETAIL_END
