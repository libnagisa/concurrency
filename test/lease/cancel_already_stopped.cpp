#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

TEST_CASE("already-stopped acquire on an empty pool completes as stopped")
{
	int_pool pool{};
	::stdexec::inplace_stop_source source{};
	source.request_stop();
	::std::optional<int_lease> got{};
	auto done = completion::pending;
	using recv = capture_receiver<int_lease, ::stdexec::inplace_stop_token>;
	auto op = ::stdexec::connect(pool.acquire(), recv{&got, &done, source.get_token()});
	::stdexec::start(op);
	CHECK(done == completion::stopped);
	CHECK_FALSE(got);
}
