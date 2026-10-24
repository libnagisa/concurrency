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
	static_assert(::nc::resource_token<connection>);

	::nc::resource_pool<connection, ::std::deque<connection>> pool{::std::deque<connection>{{3}, {5}}};

	auto permit = pool.try_acquire();
	REQUIRE(permit);
	CHECK(permit->token().fd == 5);
	permit->token().fd = 9;
	permit.reset();
	auto again = pool.try_acquire();
	REQUIRE(again);
	CHECK(again->token().fd == 9);
}
