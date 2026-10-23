#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

TEST_CASE("let_value holds the lease for nested work")
{
	int_pool pool{::std::vector<int>{9}};
	auto snd = pool.acquire()
		| ::stdexec::let_value([&pool](int_lease const& lease) noexcept {
			check_available(pool, 0);
			return ::stdexec::just(lease.token());
		});
	auto [value] = ::stdexec::sync_wait(::std::move(snd)).value();
	CHECK(value == 9);
	check_available(pool, 1);
}
