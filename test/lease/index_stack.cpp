#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

static_assert(::nc::details::available_container<::nc::index_stack<3>, ::std::size_t>);
static_assert(::nc::index_stack<0>{}.empty());
static_assert(::nc::index_stack<3>::capacity() == 3);
static_assert(!::std::is_reference_v<decltype(::std::declval<::nc::index_stack<3>&>().back())>);
static_assert([] {
	auto stack = ::nc::index_stack<1>{};
	auto index = static_cast<::std::size_t>(stack.back());
	stack.back() = index;
	stack.pop_back();
	if (!stack.empty() || index != 0)
		return false;
	stack.push_back(index);
	return !stack.empty() && stack.back() == index;
}());

TEST_CASE("index stack allocates unique indices and reuses returned holes")
{
	auto stack = ::nc::index_stack<65>{};
	auto seen = ::std::array<bool, 65>{};
	for (::std::size_t count = 0; count < seen.size(); ++count)
	{
		REQUIRE_FALSE(stack.empty());
		auto index = static_cast<::std::size_t>(stack.back());
		CHECK(stack.back() == index);
		REQUIRE(index < seen.size());
		CHECK_FALSE(seen[index]);
		seen[index] = true;
		stack.pop_back();
	}
	CHECK(stack.empty());
	stack.push_back(64);
	stack.push_back(0);
	stack.push_back(32);
	auto returned = ::std::array<bool, 65>{};
	for (auto count = 0; count < 3; ++count)
	{
		REQUIRE_FALSE(stack.empty());
		auto index = static_cast<::std::size_t>(stack.back());
		REQUIRE(index < returned.size());
		CHECK((index == 0 || index == 32 || index == 64));
		CHECK_FALSE(returned[index]);
		returned[index] = true;
		stack.pop_back();
	}
	CHECK(stack.empty());
}

TEST_CASE("index leases return indices and wake a waiting acquire")
{
	using pool_type = ::nc::lease_pool<::std::size_t, ::nc::index_stack<2>>;
	pool_type pool{};
	auto first = pool.try_acquire();
	auto second = pool.try_acquire();
	REQUIRE(first);
	REQUIRE(second);
	CHECK(first->token() != second->token());
	CHECK_FALSE(pool.try_acquire());
	auto returned_index = first->token();
	::std::optional<pool_type::lease> got{};
	auto done = completion::pending;
	auto op = ::stdexec::connect(pool.acquire(), capture_receiver<pool_type::lease>{&got, &done});
	::stdexec::start(op);
	CHECK(done == completion::pending);
	first.reset();
	REQUIRE(done == completion::value);
	REQUIRE(got);
	CHECK(got->token() == returned_index);
	CHECK_FALSE(pool.try_acquire());
	got.reset();
	auto again = pool.try_acquire();
	REQUIRE(again);
	CHECK(again->token() == returned_index);
	again.reset();
	second.reset();
	check_available(pool, 2);
}

TEST_CASE("zero capacity index pool is empty")
{
	::nc::lease_pool<::std::size_t, ::nc::index_stack<0>> pool{};
	CHECK_FALSE(pool.try_acquire());
}
