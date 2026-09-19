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

	constexpr bounded_stack(::std::from_range_t, ::std::ranges::forward_range auto&& range) noexcept
	{
		for (auto&& value : range)
			self_type::push_back(::std::forward<decltype(value)>(value));
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
};

NAGISA_BUILD_LIB_DETAIL_END

#include "./macro_pop.h"