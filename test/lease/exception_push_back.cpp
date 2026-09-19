#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "throwing_container.h"

TEST_CASE("push_back() throw from supply does not bump size")
{
	throw_ctrl ctrl{};
	boom_pool pool{throwing_stack<int>{::std::vector<int>{}, ctrl}};
	ctrl.on_push_back = true;
	CHECK_THROWS_AS(pool.supply(1), container_boom);
	CHECK_FALSE(ctrl.on_push_back);
	CHECK(pool.size() == 0);
	CHECK(pool.available() == 0);

	pool.supply(2);
	CHECK(pool.size() == 1);
	CHECK(pool.available() == 1);
	auto lease = pool.try_acquire();
	REQUIRE(lease);
	CHECK(lease->token() == 2);
}

TEST_CASE("push_back() throw from supply leaves existing tokens intact")
{
	throw_ctrl ctrl{};
	boom_pool pool{throwing_stack<int>{::std::vector<int>{3, 4}, ctrl}};
	ctrl.on_push_back = true;
	CHECK_THROWS_AS(pool.supply(5), container_boom);
	CHECK(pool.size() == 2);
	CHECK(pool.available() == 2);
	auto a = pool.try_acquire();
	REQUIRE(a);
	CHECK(a->token() == 4);
	auto b = pool.try_acquire();
	REQUIRE(b);
	CHECK(b->token() == 3);
}
