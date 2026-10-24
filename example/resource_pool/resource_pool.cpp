#include <nagisa/concurrency/semaphore.h>

#include <iostream>
#include <vector>

namespace nc = ::nagisa::concurrency;

// Token 可以是连接、buffer、句柄。permit 析构时自动还回池里。
struct connection
{
	int fd = -1;
};

auto main() -> int
{
	::nc::resource_pool<connection, ::std::vector<connection>> pool{
		::std::vector<connection>{{3}, {5}}
	};

	{
		auto permit = pool.try_acquire();
		if (!permit)
		{
			::std::cout << "pool empty\n";
			return 1;
		}
		::std::cout << "borrow fd=" << permit->token().fd << '\n';
		permit->token().fd = 9;
	}

	auto again = pool.try_acquire();
	::std::cout << "returned fd=" << again->token().fd << '\n';
}
