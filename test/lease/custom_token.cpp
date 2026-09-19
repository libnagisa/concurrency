#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

#include <deque>

TEST_CASE("custom token type with deque container")
{
	struct connection
	{
		int fd = -1;
	};
	static_assert(::nc::lease_token<connection>);

	::nc::lease_pool<connection, ::std::deque<connection>> pool{::std::deque<connection>{{3}, {5}}};
	CHECK(pool.size() == 2);
	auto lease = pool.try_acquire();
	REQUIRE(lease);
	CHECK(lease->token().fd == 5);
	lease->token().fd = 9;
	lease.reset();
	auto again = pool.try_acquire();
	REQUIRE(again);
	CHECK(again->token().fd == 9);
}
