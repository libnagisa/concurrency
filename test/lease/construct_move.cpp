#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

TEST_CASE("construct pool by moving a container")
{
	::std::vector<int> src{7, 8, 9};
	int_pool pool{::std::move(src)};
	CHECK(pool.size() == 3);
	CHECK(pool.available() == 3);
	auto lease = pool.try_acquire();
	REQUIRE(lease);
	CHECK(lease->token() == 9);
}
