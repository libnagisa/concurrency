#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "throwing_container.h"

TEST_CASE("back() throw does not consume a token")
{
	throw_ctrl ctrl{};
	boom_pool pool{throwing_stack<int>{::std::vector<int>{1, 2, 3}, ctrl}};
	ctrl.on_back = true;
	CHECK_THROWS_AS((void)pool.try_acquire(), container_boom);
	CHECK_FALSE(ctrl.on_back);
	CHECK(pool.size() == 3);
	CHECK(pool.available() == 3);
	auto a = pool.try_acquire();
	REQUIRE(a);
	CHECK(a->token() == 3);
	auto b = pool.try_acquire();
	REQUIRE(b);
	CHECK(b->token() == 2);
}
