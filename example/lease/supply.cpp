#include <nagisa/concurrency/lease.h>
#include <stdexec/execution.hpp>

#include <exception>
#include <iostream>
#include <optional>
#include <vector>

namespace nc = ::nagisa::concurrency;

using int_pool = ::nc::lease_pool<int, ::std::vector<int>>;
using int_lease = int_pool::lease;

struct capture
{
	using receiver_concept = ::stdexec::receiver_t;
	::std::optional<int_lease>* out{};
	void set_value(int_lease lease) noexcept { *out = ::std::move(lease); }
	void set_error(::std::exception_ptr) noexcept { ::std::terminate(); }
	void set_stopped() noexcept {}
	[[nodiscard]] auto get_env() const noexcept
	{
		return ::stdexec::prop{::stdexec::get_stop_token, ::stdexec::never_stop_token{}};
	}
};

auto main() -> int
{
	int_pool pool{};
	::std::optional<int_lease> got{};
	auto op = ::stdexec::connect(pool.acquire(), capture{&got});
	::stdexec::start(op);
	::std::cout << "waiting, available=" << pool.available() << '\n';

	pool.supply(77);
	::std::cout << "supply woke waiter, token=" << got->token()
		<< " available=" << pool.available() << '\n';
}
