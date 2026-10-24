#include <nagisa/concurrency/semaphore.h>
#include <stdexec/execution.hpp>

#include <iostream>
#include <memory>
#include <vector>

namespace nc = ::nagisa::concurrency;

// unique_ptr 只可移动，符合 resource_token：默认构造 + nothrow move。
using buffer = ::std::unique_ptr<int>;
using buffer_pool = ::nc::resource_pool<buffer, ::std::vector<buffer>>;

auto main() -> int
{
	::std::vector<buffer> src;
	src.push_back(::std::make_unique<int>(10));
	src.push_back(::std::make_unique<int>(20));
	buffer_pool pool{::std::move(src)};

	{
		auto permit = pool.try_acquire();
		::std::cout << "borrow *p=" << *permit->token() << '\n';
		*permit->token() += 5;
	}

	auto [permit] = ::stdexec::sync_wait(pool.acquire()).value();
	::std::cout << "acquire *p=" << *permit.token() << '\n';
}
