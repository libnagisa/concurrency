#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <array>
#include <vector>

#include <doctest/doctest.h>
#include <exec/task.hpp>

#include "support.h"

namespace war_test
{
auto increment(int& count) -> ::exec::task<void>
{
	++count;
	co_return;
}

auto await_children(int& count) -> ::exec::task<int>
{
	auto children = ::std::vector<::exec::task<void>>{};
	for (auto index = 0; index != 4; ++index)
		children.push_back(::war_test::increment(count));
	co_await ::nc::when_all_range(::std::move(children));
	co_return count;
}

auto await_failure() -> ::exec::task<bool>
{
	auto error = ::std::make_exception_ptr(::std::runtime_error{ "awaited failure" });
	auto children = ::std::array{ ::stdexec::just_error(error) };
	try
	{
		co_await ::nc::when_all_range(children);
	}
	catch (::std::runtime_error const&)
	{
		co_return true;
	}
	co_return false;
}

auto await_stopped(bool& resumed) -> ::exec::task<void>
{
	auto children = ::std::array{ ::stdexec::just_stopped() };
	co_await ::nc::when_all_range(children);
	resumed = true;
}
}

TEST_CASE("a coroutine can await a dynamic collection of move-only child tasks")
{
	auto count = 0;
	auto result = ::stdexec::sync_wait(::war_test::await_children(count));
	REQUIRE(result.has_value());
	CHECK(::std::get<0>(*result) == 4);
	CHECK(count == 4);
}

TEST_CASE("co_await exposes child errors as catchable exceptions")
{
	auto result = ::stdexec::sync_wait(::war_test::await_failure());
	REQUIRE(result.has_value());
	CHECK(::std::get<0>(*result));
}

TEST_CASE("co_await propagates stopped without resuming the coroutine body")
{
	auto resumed = false;
	CHECK_FALSE(::stdexec::sync_wait(::war_test::await_stopped(resumed)).has_value());
	CHECK_FALSE(resumed);
}
