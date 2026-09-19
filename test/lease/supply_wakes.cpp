#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

TEST_CASE("supply wakes the first waiter")
{
	int_pool pool{};
	::std::optional<int_lease> got{};
	auto done = completion::pending;
	auto op = ::stdexec::connect(pool.acquire(), capture_receiver<int_lease>{&got, &done});
	::stdexec::start(op);
	CHECK(done == completion::pending);
	pool.supply(77);
	CHECK(done == completion::value);
	REQUIRE(got);
	CHECK(got->token() == 77);
	CHECK(pool.available() == 0);
}
