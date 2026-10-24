#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"
#include "move_only_token.h"

TEST_CASE("bounded_resource_pool holds unique_ptr tokens")
{
	::std::vector<unique_int> src;
	src.push_back(::std::make_unique<int>(1));
	src.push_back(::std::make_unique<int>(2));
	::nc::bounded_resource_pool<unique_int, 2> pool{
		::std::in_place, ::std::from_range, src | ::std::views::as_rvalue
	};

	auto a = pool.try_acquire();
	auto b = pool.try_acquire();
	REQUIRE(a);
	REQUIRE(b);
	REQUIRE(a->token());
	REQUIRE(b->token());
	CHECK_FALSE(pool.try_acquire());
	a.reset();
	check_available(pool, 1);
	auto c = pool.try_acquire();
	REQUIRE(c);
	REQUIRE(c->token());
}
