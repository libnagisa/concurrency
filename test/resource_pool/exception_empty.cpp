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

	check_available(pool, 3);
	auto permit = pool.try_acquire();
	REQUIRE(permit);
	CHECK(permit->token() == 3);
}

TEST_CASE("empty() throw on an empty pool does not invent a token")
{
	throw_ctrl ctrl{};
	boom_pool pool{throwing_stack<int>{::std::vector<int>{}, ctrl}};
	ctrl.on_empty = true;
	CHECK_THROWS_AS((void)pool.try_acquire(), container_boom);

	check_available(pool, 0);
	CHECK_FALSE(pool.try_acquire());
}
