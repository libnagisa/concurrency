#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

#include <tuple>

TEST_CASE("sync_wait acquire when a token is already available")
{
	int_pool pool{::std::vector<int>{21, 22}};
	auto result = ::stdexec::sync_wait(pool.acquire());
	REQUIRE(result.has_value());
	auto lease = ::std::get<0>(::std::move(*result));
	CHECK(lease.token() == 22);
	CHECK(pool.available() == 1);
}
