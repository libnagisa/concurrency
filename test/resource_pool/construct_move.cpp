#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

TEST_CASE("construct pool by moving a container")
{
	::std::vector<int> src{7, 8, 9};
	int_pool pool{::std::move(src)};

	check_available(pool, 3);
	auto permit = pool.try_acquire();
	REQUIRE(permit);
	CHECK(permit->token() == 9);
}
