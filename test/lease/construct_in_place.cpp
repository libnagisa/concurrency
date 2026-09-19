#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

TEST_CASE("construct pool in_place with container arguments")
{
	int_pool pool{::std::in_place, 2, 42};
	CHECK(pool.size() == 2);
	CHECK(pool.available() == 2);
	auto a = pool.try_acquire();
	auto b = pool.try_acquire();
	REQUIRE(a);
	REQUIRE(b);
	CHECK(a->token() == 42);
	CHECK(b->token() == 42);
	CHECK_FALSE(pool.try_acquire());
}
