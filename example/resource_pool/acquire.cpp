#include <nagisa/concurrency/semaphore.h>
#include <stdexec/execution.hpp>

#include <iostream>
#include <vector>

namespace nc = ::nagisa::concurrency;

using int_pool = ::nc::resource_pool<int, ::std::vector<int>>;
using int_permit = int_pool::permit;

auto main() -> int
{
	int_pool pool{::std::vector<int>{21, 22}};

	{
		auto [permit] = ::stdexec::sync_wait(pool.acquire()).value();
		::std::cout << "sync_wait got " << permit.token() << '\n';
	}

	auto snd = pool.acquire()
		| ::stdexec::then([](int_permit permit) noexcept { return permit.token(); });
	auto [value] = ::stdexec::sync_wait(::std::move(snd)).value();
	::std::cout << "then got " << value << '\n';
}
