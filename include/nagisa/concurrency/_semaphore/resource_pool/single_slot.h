#pragma once

#include "./token.h"

#include "./environment.h"

#include "../macro_push.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

// 容量恰好为 1 的 available_container：一个槽加一个标志位。
// bounded_stack<T, 1> 要多带一个 size_t 和一层数组下标运算，而手写的单槽池
// 只需要 "槽里有没有东西" 这一个 bool，所以 N == 1 单独做一个。
template <class T>
struct single_slot
{
private:
	using self_type = single_slot;
public:
	using value_type = T;

	value_type _data{};
	bool _full = false;

	constexpr single_slot() = default;
	template <::std::ranges::input_range Range>
		requires ::std::default_initializable<value_type> &&
			::std::convertible_to<::std::ranges::range_reference_t<Range>, value_type> &&
			::std::is_assignable_v<value_type&, ::std::ranges::range_reference_t<Range>>
	constexpr single_slot(::std::from_range_t, Range&& range)
	{
		// Precondition: the range contains at most 1 element. Consume it only once.
		for (auto&& value : range)
		{
			NAGISA_CONCURRENCY_SEMAPHORE_ASSERT(!_full);
			_data = ::std::forward<decltype(value)>(value);
			_full = true;
		}
	}

	constexpr void push_back(value_type&& value) noexcept
	{
		NAGISA_CONCURRENCY_SEMAPHORE_ASSERT(!_full);
		_data = ::std::move(value);
		_full = true;
	}
	constexpr void push_back(value_type const& value) noexcept
	{
		NAGISA_CONCURRENCY_SEMAPHORE_ASSERT(!_full);
		_data = value;
		_full = true;
	}
	constexpr value_type& back() noexcept
	{
		NAGISA_CONCURRENCY_SEMAPHORE_ASSERT(_full);
		return _data;
	}
	constexpr void pop_back() noexcept
	{
		NAGISA_CONCURRENCY_SEMAPHORE_ASSERT(_full);
		_full = false;
	}
	[[nodiscard]] constexpr bool empty() const noexcept { return !_full; }
	[[nodiscard]] constexpr ::std::size_t size() const noexcept { return _full ? 1 : 0; }
	[[nodiscard]] constexpr static ::std::size_t capacity() noexcept { return 1; }
};

NAGISA_BUILD_LIB_DETAIL_END

#include "../macro_pop.h"
