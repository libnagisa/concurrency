#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

#include <sstream>
#include <stdexcept>
#include <string>

TEST_CASE("bounded_resource_pool has a compile-time cap")
{
	::nc::bounded_resource_pool<int, 2> pool{::std::in_place, ::std::from_range, ::std::vector<int>{1, 2}};

	check_available(pool, 2);
	auto a = pool.try_acquire();
	auto b = pool.try_acquire();
	CHECK(a);
	CHECK(b);
	CHECK_FALSE(pool.try_acquire());
	a.reset();
	check_available(pool, 1);
	CHECK(pool.try_acquire());
}

TEST_CASE("bounded stack consumes a single-pass input range")
{
	::std::istringstream input{"1 2 3"};
	auto values = ::std::views::istream<int>(input);
	using bounded_type = ::nc::bounded_resource_pool<int, 3>::available_container_type;
	static_assert(!::std::ranges::forward_range<decltype(values)>);
	auto stack = bounded_type{::std::from_range, values};
	REQUIRE(stack.size() == 3);
	CHECK(stack.back() == 3);
	stack.pop_back();
	CHECK(stack.back() == 2);
	stack.pop_back();
	CHECK(stack.back() == 1);
}

TEST_CASE("bounded stack accepts empty ranges with zero capacity")
{
	using stack_type = ::nc::bounded_resource_pool<int, 0>::available_container_type;
	constexpr auto stack = stack_type{::std::from_range, ::std::array<int, 0>{}};
	static_assert(stack.empty());
	static_assert(stack.capacity() == 0);
	CHECK(stack.empty());
}

TEST_CASE("bounded stack range constraints reject incompatible elements")
{
	using stack_type = ::nc::bounded_resource_pool<int, 2>::available_container_type;
	static_assert(::std::constructible_from<stack_type, ::std::from_range_t, ::std::array<short, 2>&>);
	static_assert(!::std::constructible_from<stack_type, ::std::from_range_t, ::std::array<::std::string, 2>&>);
	constexpr auto stack = stack_type{::std::from_range, ::std::array<short, 2>{4, 5}};
	static_assert(stack.size() == 2);
	CHECK(stack.size() == 2);
}

TEST_CASE("bounded stack propagates range dereference exceptions")
{
	using stack_type = ::nc::bounded_resource_pool<int, 2>::available_container_type;
	auto values = ::std::views::iota(0, 2) | ::std::views::transform([](int value) {
		if (value == 1)
			throw ::std::runtime_error("range read failed");
		return value;
	});
	CHECK_THROWS_AS((stack_type{::std::from_range, values}), ::std::runtime_error);
}

TEST_CASE("bounded stack propagates copy assignment exceptions")
{
	struct token
	{
		token() = default;
		token(token const&) = default;
		token(token&&) noexcept = default;
		token& operator=(token&&) noexcept = default;
		token& operator=(token const&)
		{
			throw ::std::runtime_error("copy failed");
		}
	};
	using stack_type = ::nc::bounded_resource_pool<token, 1>::available_container_type;
	auto values = ::std::array<token, 1>{};
	CHECK_THROWS_AS((stack_type{::std::from_range, values}), ::std::runtime_error);
}
