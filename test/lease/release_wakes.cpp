#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

TEST_CASE("destroying a lease wakes a waiter")
{
	int_pool pool{::std::vector<int>{3}};
	auto held = pool.try_acquire();
	CHECK(pool.available() == 0);

	::std::optional<int_lease> got{};
	auto done = completion::pending;
	auto op = ::stdexec::connect(pool.acquire(), capture_receiver<int_lease>{&got, &done});
	::stdexec::start(op);
	CHECK(done == completion::pending);

	held.reset();
	CHECK(done == completion::value);
	REQUIRE(got);
	CHECK(got->token() == 3);
}
