#include <nagisa/concurrency/lease.h>

#include <iostream>
#include <stdexcept>
#include <vector>

namespace nc = ::nagisa::concurrency;

auto main() -> int
{
	::nc::lease_pool<int, ::std::vector<int>> pool{::std::vector<int>{1, 2}};
	try
	{
		auto a = pool.try_acquire();
		auto b = pool.try_acquire();
		::std::cout << "holding 2, another borrow succeeds=" << static_cast<bool>(pool.try_acquire()) << '\n';
		throw ::std::runtime_error("work failed");
	}
	catch (::std::runtime_error const& e)
	{
		::std::cout << "caught: " << e.what() << '\n';
	}
	auto a = pool.try_acquire();
	auto b = pool.try_acquire();
	::std::cout << "after unwind both resources can be borrowed="
		<< (a.has_value() && b.has_value()) << '\n';
	return a && b ? 0 : 1;
}
