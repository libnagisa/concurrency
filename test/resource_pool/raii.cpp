#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

TEST_CASE("permit is empty by default")
{
	int_permit empty{};
	CHECK_FALSE(empty);
	CHECK(empty.token() == 0);
}

TEST_CASE("moving a permit transfers ownership")
{
	int_pool pool{::std::vector<int>{1, 2}};
	auto a = pool.try_acquire();
	REQUIRE(a);
	check_available(pool, 1);
	auto b = ::std::move(*a);
	CHECK_FALSE(*a);
	CHECK(b);
	CHECK(b.token() == 2);
	check_available(pool, 1);
}

TEST_CASE("move-assigning a permit returns the old token")
{
	int_pool pool{::std::vector<int>{1, 2}};
	auto a = pool.try_acquire();
	auto c = pool.try_acquire();
	REQUIRE(a);
	REQUIRE(c);
	CHECK(c->token() == 1);
	check_available(pool, 0);
	auto b = ::std::move(*a);
	b = ::std::move(*c);
	CHECK_FALSE(*c);
	CHECK(b.token() == 1);
	check_available(pool, 1);
}

TEST_CASE("assigning an empty permit returns the token")
{
	int_pool pool{::std::vector<int>{1, 2}};
	auto a = pool.try_acquire();
	REQUIRE(a);
	auto b = ::std::move(*a);
	b = int_permit{};
	CHECK_FALSE(b);
	check_available(pool, 2);
}
