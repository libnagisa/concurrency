#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

TEST_CASE("available tokens are LIFO")
{
	int_pool pool{::std::vector<int>{1, 2, 3}};
	auto a = pool.try_acquire();
	auto b = pool.try_acquire();
	REQUIRE(a);
	REQUIRE(b);
	CHECK(a->token() == 3);
	CHECK(b->token() == 2);
	a.reset();
	auto c = pool.try_acquire();
	REQUIRE(c);
	CHECK(c->token() == 3);
}
