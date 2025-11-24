#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <memory>
#include <functional>
#include <coroutine>
#include <queue>
#include <ranges>

#include <stdexec/execution.hpp>
#include <nagisa/concurrency/concurrency.h>

#include "./tool/timer.h"
#include "./tool/set_thread_priority.h"


namespace nc = ::nagisa::concurrency;

using clock_type = ::std::chrono::system_clock;
clock_type::time_point current_now = clock_type::now();

::nc::simple_task<void> print(int id)
{
	using ::std::chrono::duration_cast;
	using ::std::chrono::milliseconds;

	::std::cout << duration_cast<milliseconds>(clock_type::now() - current_now).count()
				<< " ms:  [coro] id=" << id
				<<  '\n';
	co_return;
}

int main() {
	using namespace ::std::chrono_literals;

	::stdexec::run_loop loop{};
	auto t = timer<decltype(loop.get_scheduler()), clock_type>(loop.get_scheduler());
	auto stop_source = ::stdexec::inplace_stop_source{};
	auto __ = ::std::jthread([&] { loop.run();  });
	::set_thread_priority(__.native_handle(), 90);
	auto _ = ::std::jthread([&] { t.run(stop_source.get_token());  });

	current_now = clock_type::now();
	auto durations = ::std::array{
			3000ms,
			100ms,
			150ms,
			500ms,
			300ms,
			2000ms,
	};
	for (auto id : ::std::views::iota(0u, durations.size()))
	{
		auto duration = durations[id];
		::nc::spawn(t.get_scheduler(current_now + duration), ::print(id));
	}
	::std::this_thread::sleep_for(5s);
	std::cout << "main: requesting stop\n";
	stop_source.request_stop();
	loop.finish();
	return 0;
}