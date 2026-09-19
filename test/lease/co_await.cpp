#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

TEST_CASE("co_await acquire when a token is available")
{
	int_pool pool{::std::vector<int>{13, 14}};
	auto take = [&pool]() -> await_task<int> {
		auto lease = co_await pool.acquire();
		co_return lease.token();
	};
	auto task = take();
	task.resume();
	REQUIRE(task.done());
	CHECK(task.take() == 14);
	CHECK(pool.available() == 2);
}
