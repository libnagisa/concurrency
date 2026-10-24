#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

TEST_CASE("cancelling a waiting acquire completes stopped and keeps the token in the pool")
{
	int_pool pool{::std::vector<int>{5}};
	auto held = pool.try_acquire();
	REQUIRE(held);

	::stdexec::inplace_stop_source source{};
	::std::optional<int_permit> got{};
	auto done = completion::pending;
	auto op = ::stdexec::connect(pool.acquire(),
		capture_receiver<int_permit, ::stdexec::inplace_stop_token>{&got, &done, source.get_token()});
	::stdexec::start(op);
	CHECK(done == completion::pending);

	source.request_stop();
	CHECK(done == completion::stopped);
	CHECK_FALSE(got.has_value());

	held.reset();
	auto again = pool.try_acquire();
	REQUIRE(again);
	CHECK(again->token() == 5);
}

TEST_CASE("an already stopped token cancels acquire even when a token is available")
{
	int_pool pool{::std::vector<int>{5}};
	::stdexec::inplace_stop_source source{};
	source.request_stop();

	::std::optional<int_permit> got{};
	auto done = completion::pending;
	auto op = ::stdexec::connect(pool.acquire(),
		capture_receiver<int_permit, ::stdexec::inplace_stop_token>{&got, &done, source.get_token()});
	::stdexec::start(op);
	CHECK(done == completion::stopped);
	CHECK_FALSE(got.has_value());

	auto kept = pool.try_acquire();
	REQUIRE(kept);
	CHECK(kept->token() == 5);
}
