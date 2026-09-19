#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"
#include "move_only_token.h"

#include <tuple>

using slot_pool = ::nc::lease_pool<move_only_slot, ::std::vector<move_only_slot>>;
using slot_lease = slot_pool::lease;

TEST_CASE("sync_wait acquire delivers a move-only token")
{
	::std::vector<move_only_slot> src;
	src.emplace_back(21);
	src.emplace_back(22);
	slot_pool pool{::std::move(src)};
	auto result = ::stdexec::sync_wait(pool.acquire());
	REQUIRE(result.has_value());
	auto lease = ::std::get<0>(::std::move(*result));
	CHECK(lease.token().id == 22);
	CHECK(pool.available() == 1);
}

TEST_CASE("then moves the move-only lease in and returns it to the pool")
{
	::std::vector<move_only_slot> src;
	src.emplace_back(8);
	slot_pool pool{::std::move(src)};
	auto snd = pool.acquire()
		| ::stdexec::then([](slot_lease lease) noexcept { return lease.token().id; });
	auto [value] = ::stdexec::sync_wait(::std::move(snd)).value();
	CHECK(value == 8);
	CHECK(pool.available() == 1);
}

TEST_CASE("supply of a move-only token wakes a waiter")
{
	slot_pool pool{};
	::std::optional<slot_lease> got{};
	auto done = completion::pending;
	auto op = ::stdexec::connect(pool.acquire(), capture_receiver<slot_lease>{&got, &done});
	::stdexec::start(op);
	CHECK(done == completion::pending);
	pool.supply(move_only_slot{77});
	CHECK(done == completion::value);
	REQUIRE(got);
	CHECK(got->token().id == 77);
}
