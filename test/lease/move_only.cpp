#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"
#include "move_only_token.h"

using slot_pool = ::nc::lease_pool<move_only_slot, ::std::vector<move_only_slot>>;

TEST_CASE("move-only token satisfies lease_token and is not copyable")
{
	static_assert(::nc::lease_token<move_only_slot>);
	static_assert(!::std::is_copy_constructible_v<move_only_slot>);
	static_assert(!::std::is_copy_assignable_v<move_only_slot>);
}

TEST_CASE("try_acquire and RAII return a move-only token")
{
	::std::vector<move_only_slot> src;
	src.emplace_back(1);
	src.emplace_back(2);
	slot_pool pool{::std::move(src)};
	CHECK(pool.size() == 2);

	{
		auto a = pool.try_acquire();
		REQUIRE(a);
		CHECK(a->token().id == 2);
		CHECK(pool.available() == 1);
		auto b = pool.try_acquire();
		REQUIRE(b);
		CHECK(b->token().id == 1);
		CHECK_FALSE(pool.try_acquire());
	}

	CHECK(pool.available() == 2);
	auto again = pool.try_acquire();
	REQUIRE(again);
	CHECK((again->token().id == 1 || again->token().id == 2));
}

TEST_CASE("moving a lease transfers a move-only token")
{
	::std::vector<move_only_slot> src;
	src.emplace_back(7);
	slot_pool pool{::std::move(src)};
	auto a = pool.try_acquire();
	REQUIRE(a);
	auto b = ::std::move(*a);
	CHECK_FALSE(*a);
	CHECK(b);
	CHECK(b.token().id == 7);
	CHECK(pool.available() == 0);
	b = {};
	CHECK(pool.available() == 1);
}
