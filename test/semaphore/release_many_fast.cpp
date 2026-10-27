#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

#include <chrono>

// release(n) 不能在锁内循环 n 次：无等待者时应是一次加法。
TEST_CASE("release of a huge batch with no waiters is O(1) under the lock")
{
	nc::counting_semaphore<> sem{0};
	auto const begin = ::std::chrono::steady_clock::now();
	sem.release(2'000'000'000);
	auto const elapsed = ::std::chrono::steady_clock::now() - begin;
	CHECK(elapsed < ::std::chrono::milliseconds(100));
	CHECK(sem.try_acquire());
}

// give_n：Store 只提供 give() 时退化成循环；提供 give(count) 时直接调用。
struct only_one
{
	int count = 0;
	void give() noexcept { ++count; }
};
struct with_bulk
{
	int count = 0;
	int bulk_calls = 0;
	void give() noexcept { ++count; }
	void give(int n) noexcept { count += n; ++bulk_calls; }
};

TEST_CASE("give_n falls back to give() and prefers give(count)")
{
	only_one a{};
	::nc::details::give_n(a, 5);
	CHECK(a.count == 5);

	with_bulk b{};
	::nc::details::give_n(b, 5);
	CHECK(b.count == 5);
	CHECK(b.bulk_calls == 1);
}
