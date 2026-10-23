#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

TEST_CASE("when_all with just_stopped unlinks a waiting acquire")
{
	int_pool pool{::std::vector<int>{1}};
	auto held = pool.try_acquire();
	REQUIRE(held);
	auto snd = ::stdexec::when_all(pool.acquire(), ::stdexec::just_stopped())
		| ::stdexec::let_stopped([] { return ::stdexec::just(); });
	::stdexec::sync_wait(::std::move(snd));
	held.reset();
	check_available(pool, 1);
}
