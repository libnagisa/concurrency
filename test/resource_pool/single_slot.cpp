#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

#include <array>

// 容量为 1 的池用单槽容器：一个槽加一个标志位，和手写的单槽池一样大。
namespace
{
	struct hand_written
	{
		::std::mutex mutex;
		int slot;
		bool has;
		void* head;
		void* tail;
	};
}
static_assert(::std::same_as<
	::nc::bounded_resource_pool<int, 1>::available_container_type,
	::nc::details::single_slot<int>>);
static_assert(!::std::same_as<
	::nc::bounded_resource_pool<int, 2>::available_container_type,
	::nc::details::single_slot<int>>);
static_assert(sizeof(::nc::bounded_resource_pool<int, 1>) == sizeof(hand_written));

TEST_CASE("single slot pool hands out its only token and takes it back")
{
	::nc::bounded_resource_pool<int, 1> pool{::std::in_place, ::std::from_range, ::std::array<int, 1>{7}};
	{
		auto first = pool.try_acquire();
		REQUIRE(first);
		CHECK(first->token() == 7);
		CHECK_FALSE(pool.try_acquire());
		first->token() = 9;
	}
	auto again = pool.try_acquire();
	REQUIRE(again);
	CHECK(again->token() == 9);
}

TEST_CASE("single slot pool hands the returned token straight to a waiter")
{
	::nc::bounded_resource_pool<int, 1> pool{::std::in_place, ::std::from_range, ::std::array<int, 1>{5}};
	auto held = pool.try_acquire();
	REQUIRE(held);

	::std::optional<::nc::bounded_resource_pool<int, 1>::permit> got{};
	auto done = completion::pending;
	auto op = ::stdexec::connect(pool.acquire(),
		capture_receiver<::nc::bounded_resource_pool<int, 1>::permit>{&got, &done});
	::stdexec::start(op);
	CHECK(done == completion::pending);

	held.reset();
	CHECK(done == completion::value);
	REQUIRE(got);
	CHECK(got->token() == 5);
	CHECK_FALSE(pool.try_acquire());
}
