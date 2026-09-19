#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <nagisa/concurrency/lease.h>
#include <stdexec/execution.hpp>

#include <memory>
#include <variant>
#include <vector>

namespace nc = ::nagisa::concurrency;

TEST_CASE("lease_token and acquire_sender concepts")
{
	static_assert(::nc::lease_token<int>);
	static_assert(::nc::lease_token<int*>);
	static_assert(::nc::lease_token<::std::monostate>);
	static_assert(::nc::lease_token<::std::unique_ptr<int>>);
	static_assert(!::std::copy_constructible<::std::unique_ptr<int>>);
	static_assert(::stdexec::sender<::nc::lease_pool<int, ::std::vector<int>>::acquire_sender>);
}
