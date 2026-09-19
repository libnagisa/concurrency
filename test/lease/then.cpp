#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

TEST_CASE("then consumes the lease and returns the token")
{
	int_pool pool{::std::vector<int>{8}};
	auto snd = pool.acquire()
		| ::stdexec::then([](int_lease lease) noexcept { return lease.token(); });
	auto [value] = ::stdexec::sync_wait(::std::move(snd)).value();
	CHECK(value == 8);
	CHECK(pool.available() == 1);
}
