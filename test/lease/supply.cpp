#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

TEST_CASE("supply grows the pool at runtime")
{
	int_pool pool{};
	CHECK(pool.size() == 0);
	pool.supply(4);
	pool.supply(5);
	CHECK(pool.size() == 2);
	CHECK(pool.available() == 2);
	auto a = pool.try_acquire();
	auto b = pool.try_acquire();
	REQUIRE(a);
	REQUIRE(b);
	CHECK(a->token() == 5);
	CHECK(b->token() == 4);
	CHECK(pool.available() == 0);
	pool.supply(6);
	CHECK(pool.size() == 3);
	auto c = pool.try_acquire();
	REQUIRE(c);
	CHECK(c->token() == 6);
}
