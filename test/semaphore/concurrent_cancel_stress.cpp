#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

#include <atomic>
#include <thread>

// 专门撞 start() 发布节点入队和订阅取消之间的竞争窗口：历史上这里曾经是一个
// use-after-free（节点发布后在锁外访问 *this 来注册 stop callback）。两个线程
// 分别并发 request_stop 和 release，反复撞这条路径。
TEST_CASE("concurrent release and cancel do not race on the waiter node")
{
	nc::counting_semaphore<1> sem{0};
	constexpr auto rounds = 2000;
	::std::atomic_int value_count{0};
	::std::atomic_int stopped_count{0};

	for (int i = 0; i < rounds; ++i)
	{
		::stdexec::inplace_stop_source source{};
		auto done = completion::pending;
		using recv = capture_receiver<::stdexec::inplace_stop_token>;
		auto op = ::stdexec::connect(sem.acquire(), recv{&done, source.get_token()});

		::std::thread canceller([&source] { source.request_stop(); });
		::std::thread releaser([&sem] { sem.release(); });
		::stdexec::start(op);
		canceller.join();
		releaser.join();

		if (done == completion::value)
		{
			value_count.fetch_add(1, ::std::memory_order_relaxed);
		}
		else
		{
			CHECK(done == completion::stopped);
			stopped_count.fetch_add(1, ::std::memory_order_relaxed);
			// release() 补回的许可留在计数里，吃掉它好让下一轮从 0 开始。
			CHECK(sem.try_acquire());
		}
	}

	CHECK(value_count.load() + stopped_count.load() == rounds);
	CHECK_FALSE(sem.try_acquire());
}
