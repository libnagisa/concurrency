#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"
#include "move_only_token.h"

using ptr_pool = ::nc::lease_pool<unique_int, ::std::vector<unique_int>>;

TEST_CASE("unique_ptr tokens keep the pointed-to object across lease cycles")
{
	static_assert(::nc::lease_token<unique_int>);
	static_assert(!::std::copy_constructible<unique_int>);

	::std::vector<unique_int> src;
	src.push_back(::std::make_unique<int>(10));
	src.push_back(::std::make_unique<int>(20));
	ptr_pool pool{::std::move(src)};

	{
		auto a = pool.try_acquire();
		REQUIRE(a);
		REQUIRE(a->token());
		CHECK(*a->token() == 20);
		*a->token() += 5;
	}

	CHECK(pool.available() == 2);
	auto b = pool.try_acquire();
	REQUIRE(b);
	REQUIRE(b->token());
	CHECK((*b->token() == 10 || *b->token() == 25));
}

TEST_CASE("supply a unique_ptr into an empty pool")
{
	ptr_pool pool{};
	pool.supply(::std::make_unique<int>(42));
	CHECK(pool.size() == 1);
	auto lease = pool.try_acquire();
	REQUIRE(lease);
	REQUIRE(lease->token());
	CHECK(*lease->token() == 42);
}
