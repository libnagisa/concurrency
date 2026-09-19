#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "throwing_container.h"

TEST_CASE("pop_back() throw restores the token already moved from back()")
{
	throw_ctrl ctrl{};
	boom_pool pool{throwing_stack<int>{::std::vector<int>{1, 2, 3}, ctrl}};
	ctrl.on_pop_back = true;
	CHECK_THROWS_AS((void)pool.try_acquire(), container_boom);
	CHECK_FALSE(ctrl.on_pop_back);
	CHECK(pool.size() == 3);
	CHECK(pool.available() == 3);

	auto a = pool.try_acquire();
	REQUIRE(a);
	CHECK(a->token() == 3);
	auto b = pool.try_acquire();
	REQUIRE(b);
	CHECK(b->token() == 2);
	auto c = pool.try_acquire();
	REQUIRE(c);
	CHECK(c->token() == 1);
}

TEST_CASE("pop_back() throw then a later try_acquire still works")
{
	throw_ctrl ctrl{};
	boom_pool pool{throwing_stack<int>{::std::vector<int>{7}, ctrl}};
	ctrl.on_pop_back = true;
	CHECK_THROWS_AS((void)pool.try_acquire(), container_boom);
	auto lease = pool.try_acquire();
	REQUIRE(lease);
	CHECK(lease->token() == 7);
	CHECK(pool.available() == 0);
}
