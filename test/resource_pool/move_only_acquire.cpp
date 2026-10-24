#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"
#include "move_only_token.h"

#include <tuple>

using slot_pool = ::nc::resource_pool<move_only_slot, ::std::vector<move_only_slot>>;
using slot_permit = slot_pool::permit;

TEST_CASE("sync_wait acquire delivers a move-only token")
{
	::std::vector<move_only_slot> src;
	src.emplace_back(21);
	src.emplace_back(22);
	slot_pool pool{::std::move(src)};
	auto result = ::stdexec::sync_wait(pool.acquire());
	REQUIRE(result.has_value());
	auto permit = ::std::get<0>(::std::move(*result));
	CHECK(permit.token().id == 22);
	check_available(pool, 1);
}

TEST_CASE("then moves the move-only permit in and returns it to the pool")
{
	::std::vector<move_only_slot> src;
	src.emplace_back(8);
	slot_pool pool{::std::move(src)};
	auto snd = pool.acquire()
		| ::stdexec::then([](slot_permit permit) noexcept { return permit.token().id; });
	auto [value] = ::stdexec::sync_wait(::std::move(snd)).value();
	CHECK(value == 8);
	check_available(pool, 1);
}

TEST_CASE("returning a move-only token wakes a waiter")
{
	::std::vector<move_only_slot> src;
	src.emplace_back(77);
	slot_pool pool{::std::move(src)};
	auto held = pool.try_acquire();
	REQUIRE(held);
	::std::optional<slot_permit> got{};
	auto done = completion::pending;
	auto op = ::stdexec::connect(pool.acquire(), capture_receiver<slot_permit>{&got, &done});
	::stdexec::start(op);
	CHECK(done == completion::pending);
	held.reset();
	CHECK(done == completion::value);
	REQUIRE(got);
	CHECK(got->token().id == 77);
}
