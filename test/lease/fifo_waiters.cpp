#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

TEST_CASE("waiting acquires are served FIFO")
{
	int_pool pool{};
	::std::optional<int_lease> first{};
	::std::optional<int_lease> second{};
	auto s1 = completion::pending;
	auto s2 = completion::pending;

	auto op1 = ::stdexec::connect(pool.acquire(), capture_receiver<int_lease>{&first, &s1});
	auto op2 = ::stdexec::connect(pool.acquire(), capture_receiver<int_lease>{&second, &s2});
	::stdexec::start(op1);
	::stdexec::start(op2);
	CHECK(s1 == completion::pending);
	CHECK(s2 == completion::pending);
	CHECK_FALSE(pool.try_acquire());

	pool.supply(1);
	CHECK(s1 == completion::value);
	CHECK(s2 == completion::pending);
	REQUIRE(first);
	CHECK(first->token() == 1);

	pool.supply(2);
	CHECK(s2 == completion::value);
	REQUIRE(second);
	CHECK(second->token() == 2);
	CHECK(pool.available() == 0);
	CHECK(pool.size() == 2);
}
