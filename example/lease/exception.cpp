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
		::std::cout << "holding 2, available=" << pool.available() << '\n';
		throw ::std::runtime_error("work failed");
	}
	catch (::std::runtime_error const& e)
	{
		::std::cout << "caught: " << e.what() << '\n';
	}
	::std::cout << "after unwind available=" << pool.available()
		<< " size=" << pool.size() << '\n';
}
