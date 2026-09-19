#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

TEST_CASE("stop token cancels a queued acquire")
{
	int_pool pool{};
	::stdexec::inplace_stop_source source{};
	::std::optional<int_lease> got{};
	auto done = completion::pending;
	using recv = capture_receiver<int_lease, ::stdexec::inplace_stop_token>;
	auto op = ::stdexec::connect(pool.acquire(), recv{&got, &done, source.get_token()});
	::stdexec::start(op);
	CHECK(done == completion::pending);

	source.request_stop();
	CHECK(done == completion::stopped);
	CHECK_FALSE(got);
	pool.supply(1);
	CHECK(pool.available() == 1);
	auto lease = pool.try_acquire();
	REQUIRE(lease);
	CHECK(lease->token() == 1);
}
