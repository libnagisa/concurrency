#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "throwing_container.h"

#include <tuple>

namespace
{
	template <class Pool>
	constexpr bool may_error = ::std::tuple_size_v<
		::stdexec::error_types_of_t<typename Pool::acquire_sender, ::stdexec::env<>, ::std::tuple>> != 0;
}

// 常规容器的 empty()/back()/pop_back() 都是 noexcept，acquire 就没有 set_error。
static_assert(!may_error<int_pool>);
static_assert(!may_error<::nc::bounded_resource_pool<int, 2>>);
// 任何一个可能抛，就多出 set_error(exception_ptr)。
static_assert(may_error<boom_pool>);

namespace
{
	struct error_receiver
	{
		using receiver_concept = ::stdexec::receiver_t;
		completion* done;
		void set_value(boom_pool::permit) noexcept { *done = completion::value; }
		void set_error(::std::exception_ptr) noexcept { *done = completion::error; }
		void set_stopped() noexcept { *done = completion::stopped; }
		[[nodiscard]] auto get_env() const noexcept
		{
			return ::stdexec::prop{::stdexec::get_stop_token, ::stdexec::never_stop_token{}};
		}
	};
}

TEST_CASE("a container that throws on acquire completes with set_error and keeps the token")
{
	throw_ctrl ctrl{};
	boom_pool pool{throwing_stack<int>{::std::vector<int>{1, 2, 3}, ctrl}};
	ctrl.on_back = true;

	auto done = completion::pending;
	auto op = ::stdexec::connect(pool.acquire(), error_receiver{&done});
	::stdexec::start(op);
	CHECK(done == completion::error);

	check_available(pool, 3);
}
