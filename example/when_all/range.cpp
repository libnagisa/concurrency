#include <iostream>
#include <ranges>

#include <stdexec/execution.hpp>

#include <nagisa/concurrency/when_all_range.h>

namespace nc = ::nagisa::concurrency;

auto main() -> int
{
	// let_value supplies the runtime task count; the view lazily constructs each sender.
	auto total = 0;
	auto work = ::stdexec::just(5)
		| ::stdexec::let_value([&](int count) {
			return ::nc::when_all_range(::std::views::iota(1, count + 1)
				| ::std::views::transform([&](int value) {
					return ::stdexec::just() | ::stdexec::then([&, value] {
						total += value;
						::std::cout << "completed item " << value << '\n';
					});
				}));
		})
		| ::stdexec::then([&] { ::std::cout << "all items completed, total=" << total << '\n'; });
	// These children finish synchronously. when_all_range itself does not create threads.
	auto completed = ::stdexec::sync_wait(::std::move(work));
	return completed.has_value() && total == 15 ? 0 : 1;
}
