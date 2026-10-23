#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

TEST_CASE("empty pool has no tokens")
{
	int_pool pool{};

	check_available(pool, 0);
	CHECK_FALSE(pool.try_acquire());
}
