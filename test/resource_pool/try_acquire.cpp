#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

TEST_CASE("try_acquire a pointer resource pool")
{
	int resources[2]{10, 20};
	::nc::resource_pool<int*, ::std::vector<int*>> pool{::std::vector<int*>{&resources[0], &resources[1]}};


	{
		auto a = pool.try_acquire();
		REQUIRE(a);
		CHECK(*a->token() == 20);
		*a->token() += 5;
		check_available(pool, 1);
		auto b = pool.try_acquire();
		REQUIRE(b);
		CHECK(*b->token() == 10);
		CHECK_FALSE(pool.try_acquire());

	}

	check_available(pool, 2);
	auto again = pool.try_acquire();
	REQUIRE(again);
	CHECK((*again->token() == 10 || *again->token() == 25));
}
