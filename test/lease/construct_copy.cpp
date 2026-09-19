#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

TEST_CASE("construct pool by copying a container")
{
	::std::vector<int> src{1, 2, 3};
	int_pool pool{src};
	CHECK(src.size() == 3);
	CHECK(pool.size() == 3);
	CHECK(pool.available() == 3);
}
