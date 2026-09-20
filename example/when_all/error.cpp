#include <iostream>
#include <ranges>
#include <stdexcept>

#include <stdexec/execution.hpp>

#include <nagisa/concurrency/when_all_range.h>

namespace nc = ::nagisa::concurrency;

auto main() -> int
{
	auto completed = 0;
	auto children = ::std::views::iota(0, 4) | ::std::views::transform([&](int index) {
		return ::stdexec::just() | ::stdexec::then([&, index] {
			if (index == 1)
				throw ::std::runtime_error{ "item 1 failed" };
			++completed;
		});
	});
	try
	{
		static_cast<void>(::stdexec::sync_wait(::nc::when_all_range(::std::move(children))));
	}
	catch (::std::runtime_error const& error)
	{
		// then does not honor stop requests; all remaining children still run before rethrow.
		::std::cout << error.what() << "; other completed items=" << completed << '\n';
		return completed == 3 ? 0 : 1;
	}
	return 1;
}
