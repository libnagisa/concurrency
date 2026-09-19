#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

TEST_CASE("bounded_lease_pool has a compile-time cap")
{
	::nc::bounded_lease_pool<int, 2> pool{};
	CHECK(pool.size() == 0);
	pool.supply(1);
	pool.supply(2);
	CHECK(pool.size() == 2);
	CHECK(pool.available() == 2);
	auto a = pool.try_acquire();
	auto b = pool.try_acquire();
	CHECK(a);
	CHECK(b);
	CHECK_FALSE(pool.try_acquire());
	a.reset();
	CHECK(pool.available() == 1);
	CHECK(pool.try_acquire());
}
