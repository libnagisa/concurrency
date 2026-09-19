#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

#include <atomic>
#include <thread>
#include <tuple>

TEST_CASE("bounded pool limits concurrent holders")
{
	::nc::bounded_lease_pool<int, 2> pool{};
	pool.supply(0);
	pool.supply(1);

	::std::atomic_int in_flight{0};
	::std::atomic_int max_flight{0};
	::std::atomic_int completions{0};
	::std::atomic_int failures{0};
	constexpr auto thread_count = 4;
	constexpr auto per_thread = 30;

	auto worker = [&] {
		for (int i = 0; i < per_thread; ++i)
		{
			auto result = ::stdexec::sync_wait(pool.acquire());
			if (!result.has_value())
			{
				failures.fetch_add(1, ::std::memory_order_relaxed);
				continue;
			}
			auto lease = ::std::get<0>(::std::move(*result));
			int const now = in_flight.fetch_add(1, ::std::memory_order_acq_rel) + 1;
			for (int prev = max_flight.load(::std::memory_order_relaxed);
				prev < now && !max_flight.compare_exchange_weak(prev, now, ::std::memory_order_relaxed);)
			{
			}
			::std::this_thread::yield();
			in_flight.fetch_sub(1, ::std::memory_order_acq_rel);
			completions.fetch_add(1, ::std::memory_order_relaxed);
			(void)lease;
		}
	};

	::std::vector<::std::thread> threads{};
	threads.reserve(thread_count);
	for (int i = 0; i < thread_count; ++i)
		threads.emplace_back(worker);
	for (auto& thread : threads)
		thread.join();

	CHECK(failures.load() == 0);
	CHECK(completions.load() == thread_count * per_thread);
	CHECK(max_flight.load() <= 2);
	CHECK(in_flight.load() == 0);
	CHECK(pool.available() == 2);
	CHECK(pool.size() == 2);
}
