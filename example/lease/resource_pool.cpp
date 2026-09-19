#include <nagisa/concurrency/lease.h>

#include <iostream>
#include <vector>

namespace nc = ::nagisa::concurrency;

// Token 可以是连接、buffer、句柄。lease 析构时自动还回池里。
struct connection
{
	int fd = -1;
};

auto main() -> int
{
	::nc::lease_pool<connection, ::std::vector<connection>> pool{
		::std::vector<connection>{{3}, {5}}
	};

	{
		auto lease = pool.try_acquire();
		if (!lease)
		{
			::std::cout << "pool empty\n";
			return 1;
		}
		::std::cout << "borrow fd=" << lease->token().fd
			<< " available=" << pool.available() << '\n';
		lease->token().fd = 9;
	}

	auto again = pool.try_acquire();
	::std::cout << "returned fd=" << again->token().fd
		<< " available=" << pool.available() << '\n';
}
