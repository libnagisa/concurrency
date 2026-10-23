#include <nagisa/concurrency/lease.h>

#include <iostream>
#include <variant>
#include <array>

namespace nc = ::nagisa::concurrency;

// Token 不必带资源，用 monostate 当「许可」即可把并发限制在 N 个。
auto main() -> int
{
	::nc::bounded_lease_pool<::std::monostate, 2> pool{
		::std::in_place, ::std::from_range, ::std::array<::std::monostate, 2>{}
	};

	auto a = pool.try_acquire();
	auto b = pool.try_acquire();
	::std::cout << "held 2 permits, third try "
		<< (pool.try_acquire() ? "succeeded" : "failed") << '\n';

	a.reset();
	::std::cout << "released one" << '\n';
	auto c = pool.try_acquire();
	::std::cout << "third try " << (c ? "succeeded" : "failed") << '\n';
}
