#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <array>
#include <string>

#include <doctest/doctest.h>

#include "support.h"

TEST_CASE("all children start and out-of-order completion waits for the last child")
{
	auto states = ::std::array<::war_test::child_state, 3>{};
	auto children = states | ::std::views::transform([](auto&& state) {
		return ::war_test::controlled_sender{ state };
	});
	auto result = ::war_test::result{};
	auto operation = ::stdexec::connect(::nc::when_all_range(children), ::war_test::receiver{ result });
	::stdexec::start(operation);
	for (auto&& state : states)
		CHECK(state.starts == 1);
	CHECK(result.calls == 0);
	states[2].finish(::war_test::completion::value);
	states[0].finish(::war_test::completion::value);
	CHECK(result.calls == 0);
	states[1].finish(::war_test::completion::value);
	CHECK(result.calls == 1);
	CHECK(result.done == ::war_test::completion::value);
}

TEST_CASE("error requests sibling stop but still waits for non-cooperative work")
{
	auto states = ::std::array<::war_test::child_state, 3>{};
	states[0].error = ::std::make_exception_ptr(::std::runtime_error{ "first failure" });
	states[1].stop_on_request = true;
	auto children = states | ::std::views::transform([](auto&& state) {
		return ::war_test::controlled_sender{ state };
	});
	auto result = ::war_test::result{};
	auto operation = ::stdexec::connect(::nc::when_all_range(children), ::war_test::receiver{ result });
	::stdexec::start(operation);
	states[0].finish(::war_test::completion::error);
	CHECK(states[1].completions == 1);
	CHECK(states[2].token.stop_requested());
	CHECK(result.calls == 0);
	states[2].finish(::war_test::completion::value);
	CHECK(result.calls == 1);
	CHECK(result.done == ::war_test::completion::error);
	CHECK(result.error == states[0].error);
}

TEST_CASE("error wins over stopped in either completion order")
{
	auto states = ::std::array<::war_test::child_state, 2>{};
	states[1].error = ::std::make_exception_ptr(::std::runtime_error{ "failure" });
	auto children = states | ::std::views::transform([](auto&& state) {
		return ::war_test::controlled_sender{ state };
	});
	auto result = ::war_test::result{};
	auto operation = ::stdexec::connect(::nc::when_all_range(children), ::war_test::receiver{ result });
	::stdexec::start(operation);
	SUBCASE("stopped then error")
	{
		states[0].finish(::war_test::completion::stopped);
		CHECK(result.calls == 0);
		states[1].finish(::war_test::completion::error);
	}
	SUBCASE("error then stopped")
	{
		states[1].finish(::war_test::completion::error);
		CHECK(result.calls == 0);
		states[0].finish(::war_test::completion::stopped);
	}
	CHECK(result.calls == 1);
	CHECK(result.done == ::war_test::completion::error);
	CHECK(result.error == states[1].error);
}

TEST_CASE("the first reported error is retained when several children fail")
{
	auto states = ::std::array<::war_test::child_state, 2>{};
	states[0].error = ::std::make_exception_ptr(::std::runtime_error{ "later" });
	states[1].error = ::std::make_exception_ptr(::std::runtime_error{ "first" });
	auto children = states | ::std::views::transform([](auto&& state) {
		return ::war_test::controlled_sender{ state };
	});
	auto result = ::war_test::result{};
	auto operation = ::stdexec::connect(::nc::when_all_range(children), ::war_test::receiver{ result });
	::stdexec::start(operation);
	states[1].finish(::war_test::completion::error);
	states[0].finish(::war_test::completion::error);
	CHECK(result.calls == 1);
	CHECK(result.error == states[1].error);
}

TEST_CASE("synchronous failure still starts every remaining child")
{
	auto states = ::std::array<::war_test::child_state, 3>{};
	states[0].on_start = ::war_test::completion::error;
	states[0].error = ::std::make_exception_ptr(::std::runtime_error{ "immediate" });
	states[1].stop_on_request = true;
	states[2].on_start = ::war_test::completion::value;
	auto children = states | ::std::views::transform([](auto&& state) {
		return ::war_test::controlled_sender{ state };
	});
	auto result = ::war_test::result{};
	auto operation = ::stdexec::connect(::nc::when_all_range(children), ::war_test::receiver{ result });
	::stdexec::start(operation);
	for (auto&& state : states)
	{
		CHECK(state.starts == 1);
		CHECK(state.completions == 1);
	}
	CHECK(states[1].token.stop_requested());
	CHECK(result.calls == 1);
	CHECK(result.error == states[0].error);
}

TEST_CASE("sync_wait rethrows the original child exception")
{
	auto error = ::std::make_exception_ptr(::std::runtime_error{ "child failure" });
	auto children = ::std::array{ ::stdexec::just_error(error) };
	CHECK_THROWS_WITH_AS(::stdexec::sync_wait(::nc::when_all_range(children)),
		"child failure", ::std::runtime_error);
}
