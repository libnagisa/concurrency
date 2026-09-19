#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"
#include "move_only_token.h"

TEST_CASE("bounded_lease_pool holds unique_ptr tokens")
{
	::nc::bounded_lease_pool<unique_int, 2> pool{};
	pool.supply(::std::make_unique<int>(1));
	pool.supply(::std::make_unique<int>(2));
	CHECK(pool.size() == 2);
	auto a = pool.try_acquire();
	auto b = pool.try_acquire();
	REQUIRE(a);
	REQUIRE(b);
	REQUIRE(a->token());
	REQUIRE(b->token());
	CHECK_FALSE(pool.try_acquire());
	a.reset();
	CHECK(pool.available() == 1);
	auto c = pool.try_acquire();
	REQUIRE(c);
	REQUIRE(c->token());
}
