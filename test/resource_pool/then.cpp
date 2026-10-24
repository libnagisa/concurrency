#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

TEST_CASE("then consumes the permit and returns the token")
{
	int_pool pool{::std::vector<int>{8}};
	auto snd = pool.acquire()
		| ::stdexec::then([](int_permit permit) noexcept { return permit.token(); });
	auto [value] = ::stdexec::sync_wait(::std::move(snd)).value();
	CHECK(value == 8);
	check_available(pool, 1);
}
