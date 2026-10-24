#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

#include <atomic>
#include <thread>

// 并发 request_stop 与归还 token 争同一个等待节点：节点只能被其中一方完成，
// 且 token 要么交给等待者、要么留在池里，不能丢也不能多出来。
TEST_CASE("concurrent return and cancel never lose or duplicate the token")
{
	constexpr auto rounds = 2000;
	int_pool pool{::std::vector<int>{42}};
	int values = 0;
	int stoppeds = 0;

	for (int i = 0; i < rounds; ++i)
	{
		auto held = pool.try_acquire();
		REQUIRE(held);
		::stdexec::inplace_stop_source source{};
		::std::optional<int_permit> got{};
		auto done = completion::pending;
		using recv = capture_receiver<int_permit, ::stdexec::inplace_stop_token>;
		auto op = ::stdexec::connect(pool.acquire(), recv{&got, &done, source.get_token()});

		::std::thread canceller([&source] { source.request_stop(); });
		::std::thread returner([&held] { held.reset(); });
		::stdexec::start(op);
		canceller.join();
		returner.join();

		if (done == completion::value)
		{
			++values;
			REQUIRE(got);
			CHECK(got->token() == 42);
			CHECK_FALSE(pool.try_acquire());
			got.reset();
		}
		else
		{
			CHECK(done == completion::stopped);
			++stoppeds;
		}
		auto back = pool.try_acquire();
		REQUIRE(back);
		CHECK(back->token() == 42);
	}
	CHECK(values + stoppeds == rounds);
}
