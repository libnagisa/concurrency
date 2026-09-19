
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <memory>
#include <functional>
#include <coroutine>
#include <queue>
#include <ranges>
#include <functional>
#include <coroutine>
#include <random>
#include <array>
#include <algorithm>
#include <string>

#include <stdexec/execution.hpp>
#include <nagisa/concurrency/concurrency.h>
#include <exec/async_scope.hpp>

#include "./tool/timer.h"
#include "./tool/set_thread_priority.h"
#include "./tool/erased_task.h"
#include "./tool/heavy_cpu.h"
#include "./tool/static_thread_pool.h"

using clock_type = ::std::chrono::steady_clock;
using ::std::chrono::duration_cast;
using ::std::chrono::milliseconds;
using ::std::chrono_literals::operator ""ms;

erased_task run_loop(::std::function<void()> process)
{

	auto scheduler = co_await ::stdexec::get_scheduler();
	auto stop_token = co_await ::stdexec::get_stop_token();

	while (!stop_token.stop_requested())
	{
		auto schedule = ::stdexec::schedule(scheduler);
		process();
		co_await ::std::move(schedule);
	}
	co_return;
}

inline auto format_range(auto const& diff)
{
	auto result = ::std::string("{");
	if (diff.size())
		result += ::std::to_string(diff.front());
	for (auto i : diff | ::std::views::drop(1))
	{
		result += ", " + ::std::to_string(i);
	}
	result += "}";
	return result;
};

static volatile ::std::atomic_size_t count = 0;
int main()
{
	constexpr auto payload = 3u;
	constexpr auto interval = 30ms;
	constexpr auto task_count = 20u;
	constexpr auto thread_count = 20u;

	std::random_device rd;
	std::mt19937 gen{ rd() };
	auto dist = std::uniform_int_distribution{ static_cast<decltype(payload)>(payload * 0.9), static_cast<decltype(payload)>(payload * 1.1) };
	auto diffs = ::std::array<::std::vector<long long>, task_count>{};
	auto run_count = ::std::array<::std::size_t, task_count>{};
	{
		auto pool = static_thread_pool(thread_count);
		for (auto&& thread : pool.threads())
		{
			::set_thread_priority(thread.native_handle(), 90);
		}
		{
			auto timer = ::join_timer<clock_type>(pool.get_scheduler());
			{
				auto scope = ::exec::async_scope{};

				for (auto i : ::std::views::iota(0u, task_count))
				{
					clock_type::time_point time = clock_type::now() - 30ms;
					auto task = ::run_loop([&, i, time] mutable
						{
							auto duration = duration_cast<milliseconds>(clock_type::now() - time);
							if (auto d = duration - interval; d.count() < 0 || d.count() > 1)
							{
								diffs[i].push_back(d.count());
							}
							time = clock_type::now();
							count += ::heavy_cpu(dist(gen));
							run_count[i]++;
						});
					::nc::set_scheduler(task.handle().promise(), timer.get_scheduler(interval));
					::nc::set_stop_token(task.handle().promise(), scope.get_stop_token());
					scope.spawn(::stdexec::starts_on(pool.get_scheduler(), ::std::move(task)));
				}

				::std::this_thread::sleep_for(3000ms);
				::std::cout << "request stop" << ::std::endl;
				scope.request_stop();
				::stdexec::sync_wait(scope.on_empty());
			}
		}
	}
	::std::ranges::for_each(diffs, [](::std::span<long long> diff) { ::std::cout << ::format_range(diff) << ::std::endl; });
	auto score = 0.0;
	auto index = 0;
	for (auto&& diff : diffs)
	{
		auto view = diff | ::std::views::transform([](long long i) { return i * i; });
		auto sq = ::std::accumulate(view.begin(), view.end(), 0.0);
		score += ::std::sqrt(sq) / static_cast<double>(run_count[index]);
		++index;
	}
	score /= task_count;
	::std::cout << "score: " << score << ::std::endl;

	return 0;
}