#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

#include <atomic>
#include <thread>

TEST_CASE("each token is held by at most one thread at a time")
{
	constexpr auto thread_count = 4;
	constexpr auto per_thread = 2000;
	int_pool pool{::std::vector<int>{0, 1}};
	::std::atomic_int holders[2] = {};
	::std::atomic_int violations{0};
	::std::atomic_int completions{0};

	auto worker = [&] {
		for (int i = 0; i < per_thread; ++i)
		{
			auto result = ::stdexec::sync_wait(pool.acquire());
			if (!result.has_value())
				continue;
			auto permit = ::std::get<0>(::std::move(*result));
			auto const id = permit.token();
			if (holders[id].fetch_add(1, ::std::memory_order_acq_rel) != 0)
				violations.fetch_add(1, ::std::memory_order_relaxed);
			::std::this_thread::yield();
			holders[id].fetch_sub(1, ::std::memory_order_acq_rel);
			completions.fetch_add(1, ::std::memory_order_relaxed);
		}
	};

	::std::vector<::std::thread> threads{};
	for (int i = 0; i < thread_count; ++i)
		threads.emplace_back(worker);
	for (auto& thread : threads)
		thread.join();

	CHECK(violations.load() == 0);
	CHECK(completions.load() == thread_count * per_thread);
	check_available(pool, 2);
}
