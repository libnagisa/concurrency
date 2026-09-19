#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "throwing_container.h"

TEST_CASE("empty() throw leaves tokens untouched")
{
	throw_ctrl ctrl{};
	boom_pool pool{throwing_stack<int>{::std::vector<int>{1, 2, 3}, ctrl}};
	ctrl.on_empty = true;
	CHECK_THROWS_AS((void)pool.try_acquire(), container_boom);
	CHECK_FALSE(ctrl.on_empty);
	CHECK(pool.size() == 3);
	CHECK(pool.available() == 3);
	auto lease = pool.try_acquire();
	REQUIRE(lease);
	CHECK(lease->token() == 3);
}

TEST_CASE("empty() throw on an empty pool does not invent a token")
{
	throw_ctrl ctrl{};
	boom_pool pool{throwing_stack<int>{::std::vector<int>{}, ctrl}};
	ctrl.on_empty = true;
	CHECK_THROWS_AS((void)pool.try_acquire(), container_boom);
	CHECK(pool.size() == 0);
	CHECK(pool.available() == 0);
	CHECK_FALSE(pool.try_acquire());
}
