#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <array>
#include <concepts>
#include <memory>
#include <ranges>
#include <span>
#include <tuple>
#include <type_traits>
#include <vector>

#include <doctest/doctest.h>

#include "support.h"

TEST_CASE("empty range completes successfully without a value")
{
	auto children = ::std::vector<decltype(::stdexec::just())>{};
	auto result = ::stdexec::sync_wait(::nc::when_all_range(::std::move(children)));
	static_assert(::std::same_as<decltype(result), ::std::optional<::std::tuple<>>>);
	REQUIRE(result.has_value());
}

TEST_CASE("single and many synchronous children each run once")
{
	for (auto count : { 1, 8, 256 })
	{
		CAPTURE(count);
		auto calls = ::std::vector<int>(count, 0);
		auto children = ::std::views::iota(0, count)
			| ::std::views::transform([&](int index) {
				return ::stdexec::just() | ::stdexec::then([&, index] { ++calls[index]; });
			});
		auto result = ::stdexec::sync_wait(::nc::when_all_range(::std::move(children)));
		REQUIRE(result.has_value());
		for (auto called : calls)
			CHECK(called == 1);
	}
}

TEST_CASE("transform view is evaluated at connect and work begins at start")
{
	auto created = 0;
	auto started = 0;
	auto children = ::std::views::iota(0, 3)
		| ::std::views::transform([&](int) {
			++created;
			return ::stdexec::just() | ::stdexec::then([&] { ++started; });
		});
	auto sender = ::nc::when_all_range(::std::move(children));
	CHECK(created == 0);
	CHECK(started == 0);
	auto result = ::war_test::result{};
	auto operation = ::stdexec::connect(::std::move(sender), ::war_test::receiver{ result });
	CHECK(created == 3);
	CHECK(started == 0);
	CHECK(result.calls == 0);
	::stdexec::start(operation);
	CHECK(started == 3);
	CHECK(result.calls == 1);
	CHECK(result.done == ::war_test::completion::value);
}

TEST_CASE("rvalue container owns move-only children until connection")
{
	auto total = 0;
	auto make_child = [&](int value) {
		return ::stdexec::just(::std::make_unique<int>(value))
			| ::stdexec::then([&](::std::unique_ptr<int> number) { total += *number; });
	};
	auto make_sender = [&] {
		auto children = ::std::vector<decltype(make_child(0))>{};
		children.push_back(make_child(2));
		children.push_back(make_child(3));
		return ::nc::when_all_range(::std::move(children));
	};
	auto sender = make_sender();
	static_assert(!::std::copy_constructible<decltype(sender)>);
	CHECK(total == 0);
	REQUIRE(::stdexec::sync_wait(::std::move(sender)).has_value());
	CHECK(total == 5);
}

TEST_CASE("lvalue array and span borrow and consume move-only children")
{
	auto consumed = 0;
	auto make_child = [&] {
		return ::stdexec::just(::std::make_unique<int>(7))
			| ::stdexec::then([&](::std::unique_ptr<int> value) { consumed += *value; });
	};
	auto children = ::std::array{ make_child(), make_child() };
	SUBCASE("lvalue range")
	{
		REQUIRE(::stdexec::sync_wait(::nc::when_all_range(children)).has_value());
	}
	SUBCASE("borrowed view")
	{
		REQUIRE(::stdexec::sync_wait(::nc::when_all_range(::std::span{ children })).has_value());
	}
	CHECK(consumed == 14);
}

TEST_CASE("let_value constructs a dynamic range and then runs after every child")
{
	auto completed = 0;
	auto sender = ::stdexec::just(5)
		| ::stdexec::let_value([&](int count) {
			return ::nc::when_all_range(::std::views::iota(0, count)
				| ::std::views::transform([&](int) {
					return ::stdexec::just() | ::stdexec::then([&] { ++completed; });
				}));
		})
		| ::stdexec::then([&] { return completed; });
	auto result = ::stdexec::sync_wait(::std::move(sender));
	REQUIRE(result.has_value());
	CHECK(::std::get<0>(*result) == 5);
}
