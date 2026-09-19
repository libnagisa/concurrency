#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "throwing_container.h"

TEST_CASE("size() throw from available() does not change the pool")
{
	throw_ctrl ctrl{};
	boom_pool pool{throwing_stack<int>{::std::vector<int>{1, 2}, ctrl}};
	ctrl.on_size = true;
	CHECK_THROWS_AS((void)pool.available(), container_boom);
	CHECK_FALSE(ctrl.on_size);
	CHECK(pool.size() == 2);
	CHECK(pool.available() == 2);
	auto lease = pool.try_acquire();
	REQUIRE(lease);
	CHECK(lease->token() == 2);
}
