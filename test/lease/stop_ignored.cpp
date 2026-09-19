#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

TEST_CASE("available token is given even if stop was already requested")
{
	int_pool pool{::std::vector<int>{4}};
	::stdexec::inplace_stop_source source{};
	source.request_stop();
	::std::optional<int_lease> got{};
	auto done = completion::pending;
	using recv = capture_receiver<int_lease, ::stdexec::inplace_stop_token>;
	auto op = ::stdexec::connect(pool.acquire(), recv{&got, &done, source.get_token()});
	::stdexec::start(op);
	CHECK(done == completion::value);
	REQUIRE(got);
	CHECK(got->token() == 4);
}
