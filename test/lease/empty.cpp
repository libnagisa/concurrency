#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

TEST_CASE("empty pool has no tokens")
{
	int_pool pool{};
	CHECK(pool.size() == 0);
	CHECK(pool.available() == 0);
	CHECK_FALSE(pool.try_acquire());
}
