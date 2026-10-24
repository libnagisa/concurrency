#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

TEST_CASE("a returned token goes straight to the first waiter, not back to the container")
{
	int_pool pool{::std::vector<int>{7}};
	auto held = pool.try_acquire();
	REQUIRE(held);

	::std::optional<int_permit> first{};
	::std::optional<int_permit> second{};
	auto first_done = completion::pending;
	auto second_done = completion::pending;
	auto op1 = ::stdexec::connect(pool.acquire(), capture_receiver<int_permit>{&first, &first_done});
	auto op2 = ::stdexec::connect(pool.acquire(), capture_receiver<int_permit>{&second, &second_done});
	::stdexec::start(op1);
	::stdexec::start(op2);

	held.reset();
	CHECK(first_done == completion::value);
	CHECK(second_done == completion::pending);
	CHECK_FALSE(pool.try_acquire());
	REQUIRE(first);
	CHECK(first->token() == 7);

	first.reset();
	CHECK(second_done == completion::value);
	REQUIRE(second);
	CHECK(second->token() == 7);
	CHECK_FALSE(pool.try_acquire());
}
