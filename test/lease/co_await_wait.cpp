#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

TEST_CASE("co_await acquire suspends until a lease is released")
{
	int_pool pool{::std::vector<int>{42}};
	auto held = pool.try_acquire();
	CHECK(pool.available() == 0);

	auto take = [&pool]() -> await_task<int> {
		auto lease = co_await pool.acquire();
		co_return lease.token();
	};
	auto task = take();
	task.resume();
	CHECK_FALSE(task.done());

	held.reset();
	REQUIRE(task.done());
	CHECK(task.take() == 42);
	CHECK(pool.available() == 1);
}
