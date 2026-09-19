#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

TEST_CASE("lease is empty by default")
{
	int_lease empty{};
	CHECK_FALSE(empty);
	CHECK(empty.token() == 0);
}

TEST_CASE("moving a lease transfers ownership")
{
	int_pool pool{::std::vector<int>{1, 2}};
	auto a = pool.try_acquire();
	REQUIRE(a);
	CHECK(pool.available() == 1);
	auto b = ::std::move(*a);
	CHECK_FALSE(*a);
	CHECK(b);
	CHECK(b.token() == 2);
	CHECK(pool.available() == 1);
}

TEST_CASE("move-assigning a lease returns the old token")
{
	int_pool pool{::std::vector<int>{1, 2}};
	auto a = pool.try_acquire();
	auto c = pool.try_acquire();
	REQUIRE(a);
	REQUIRE(c);
	CHECK(c->token() == 1);
	CHECK(pool.available() == 0);
	auto b = ::std::move(*a);
	b = ::std::move(*c);
	CHECK_FALSE(*c);
	CHECK(b.token() == 1);
	CHECK(pool.available() == 1);
}

TEST_CASE("assigning an empty lease returns the token")
{
	int_pool pool{::std::vector<int>{1, 2}};
	auto a = pool.try_acquire();
	REQUIRE(a);
	auto b = ::std::move(*a);
	b = int_lease{};
	CHECK_FALSE(b);
	CHECK(pool.available() == 2);
}
