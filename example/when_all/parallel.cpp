#include <iostream>
#include <ranges>
#include <vector>

#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

#include <nagisa/concurrency/when_all_range.h>

namespace nc = ::nagisa::concurrency;

auto main() -> int
{
	auto pool = ::exec::static_thread_pool{ 4 };
	auto scheduler = pool.get_scheduler();
	auto squares = ::std::vector<int>(8);
	// Each child owns one result slot. Keep the vector and pool alive until sync_wait returns.
	auto children = ::std::views::iota(::std::size_t{ 0 }, squares.size())
		| ::std::views::transform([&](::std::size_t index) {
			return ::stdexec::schedule(scheduler) | ::stdexec::then([&, index] {
				auto value = static_cast<int>(index + 1);
				squares[index] = value * value;
			});
		});
	if (!::stdexec::sync_wait(::nc::when_all_range(::std::move(children))))
		return 1;

	auto total = 0;
	for (auto value : squares)
	{
		::std::cout << value << ' ';
		total += value;
	}
	::std::cout << "\nall parallel tasks completed, sum=" << total << '\n';
	return total == 204 ? 0 : 1;
}
