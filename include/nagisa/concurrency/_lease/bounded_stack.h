#pragma once

#include "./token.h"

#include "./environment.h"

#include "./macro_push.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

template <class T, ::std::size_t N>
struct bounded_stack
{
private:
	using self_type = bounded_stack;
public:
	using value_type = T;

	::std::array<value_type, N> _data{};
	::std::size_t _size = 0;

	template <::std::ranges::input_range Range>
		requires ::std::default_initializable<value_type> &&
			::std::convertible_to<::std::ranges::range_reference_t<Range>, value_type> &&
			::std::is_assignable_v<value_type&, ::std::ranges::range_reference_t<Range>>
	constexpr bounded_stack(::std::from_range_t, Range&& range)
	{
		// Precondition: the range contains at most N elements. Consume it only once.
		for (auto&& value : range)
		{
			NAGISA_CONCURRENCY_LEASE_ASSERT(_size < N);
			_data[_size] = ::std::forward<decltype(value)>(value);
			++_size;
		}
	}

	constexpr void push_back(value_type&& value) noexcept
	{
		NAGISA_CONCURRENCY_LEASE_ASSERT(_size < N);
		_data[_size++] = ::std::move(value);
	}
	constexpr void push_back(value_type const& value) noexcept
	{
		NAGISA_CONCURRENCY_LEASE_ASSERT(_size < N);
		_data[_size++] = value;
	}
	constexpr value_type& back() noexcept
	{
		NAGISA_CONCURRENCY_LEASE_ASSERT(_size);
		return _data[_size - 1];
	}
	constexpr void pop_back() noexcept 
	{
		NAGISA_CONCURRENCY_LEASE_ASSERT(_size);
		--_size; 
	}
	[[nodiscard]] constexpr bool empty() const noexcept { return _size == 0; }
	[[nodiscard]] constexpr ::std::size_t size() const noexcept { return _size; }
	[[nodiscard]] constexpr static ::std::size_t capacity() noexcept { return N; }
};

NAGISA_BUILD_LIB_DETAIL_END

#include "./macro_pop.h"