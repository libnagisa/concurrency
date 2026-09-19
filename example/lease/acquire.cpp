#include <nagisa/concurrency/lease.h>
#include <stdexec/execution.hpp>

#include <iostream>
#include <vector>

namespace nc = ::nagisa::concurrency;

using int_pool = ::nc::lease_pool<int, ::std::vector<int>>;
using int_lease = int_pool::lease;

auto main() -> int
{
	int_pool pool{::std::vector<int>{21, 22}};

	{
		auto [lease] = ::stdexec::sync_wait(pool.acquire()).value();
		::std::cout << "sync_wait got " << lease.token()
			<< ", available=" << pool.available() << '\n';
	}

	auto snd = pool.acquire()
		| ::stdexec::then([](int_lease lease) noexcept { return lease.token(); });
	auto [value] = ::stdexec::sync_wait(::std::move(snd)).value();
	::std::cout << "then got " << value
		<< ", available=" << pool.available() << '\n';
}
