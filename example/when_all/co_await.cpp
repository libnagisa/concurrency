#include <iostream>
#include <vector>

#include <exec/task.hpp>
#include <stdexec/execution.hpp>

#include <nagisa/concurrency/when_all_range.h>

namespace nc = ::nagisa::concurrency;

auto fill_result(int value, int& output) -> ::exec::task<void>
{
	output = value * 10;
	co_return;
}

auto run_batch(int count) -> ::exec::task<int>
{
	auto results = ::std::vector<int>(count);
	auto children = ::std::vector<::exec::task<void>>{};
	children.reserve(results.size());
	for (auto index = 0; index != count; ++index)
		children.push_back(::fill_result(index + 1, results[index]));

	// Consume the move-only tasks and resume only after every child has finished.
	co_await ::nc::when_all_range(::std::move(children));
	auto total = 0;
	for (auto value : results)
		total += value;
	co_return total;
}

auto main() -> int
{
	auto result = ::stdexec::sync_wait(::run_batch(4));
	if (!result)
		return 1;
	auto [total] = *result;
	::std::cout << "co_await completed, total=" << total << '\n';
	return total == 100 ? 0 : 1;
}
