#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <array>
#include <vector>

#include <doctest/doctest.h>

#include "support.h"

TEST_CASE("empty range observes an already requested outer stop")
{
	auto source = ::stdexec::inplace_stop_source{};
	source.request_stop();
	auto result = ::war_test::result{};
	auto operation = ::stdexec::connect(
		::nc::when_all_range(::std::vector<decltype(::stdexec::just())>{}),
		::war_test::receiver{ result, source.get_token() });
	CHECK(result.calls == 0);
	::stdexec::start(operation);
	CHECK(result.calls == 1);
	CHECK(result.done == ::war_test::completion::stopped);
}

TEST_CASE("outer stop reaches every child before start or while waiting")
{
	auto states = ::std::array<::war_test::child_state, 3>{};
	for (auto&& state : states)
		state.stop_on_request = true;
	auto source = ::stdexec::inplace_stop_source{};
	auto children = states | ::std::views::transform([](auto&& state) {
		return ::war_test::controlled_sender{ state };
	});
	auto result = ::war_test::result{};
	auto operation = ::stdexec::connect(::nc::when_all_range(children),
		::war_test::receiver{ result, source.get_token() });
	SUBCASE("already stopped")
	{
		source.request_stop();
		::stdexec::start(operation);
	}
	SUBCASE("stop pending work synchronously from the callback")
	{
		::stdexec::start(operation);
		CHECK(result.calls == 0);
		source.request_stop();
	}
	CHECK(result.calls == 1);
	CHECK(result.done == ::war_test::completion::stopped);
	for (auto&& state : states)
	{
		CHECK(state.starts == 1);
		CHECK(state.completions == 1);
		CHECK(state.token.stop_requested());
	}
	source.request_stop();
	CHECK(source.stop_requested());
	CHECK(result.calls == 1);
}

TEST_CASE("outer stop waits for children that ignore cancellation")
{
	auto state = ::war_test::child_state{};
	auto source = ::stdexec::inplace_stop_source{};
	auto children = ::std::array{ ::war_test::controlled_sender{ state } };
	auto result = ::war_test::result{};
	auto operation = ::stdexec::connect(::nc::when_all_range(children),
		::war_test::receiver{ result, source.get_token() });
	::stdexec::start(operation);
	source.request_stop();
	CHECK(state.token.stop_requested());
	CHECK(result.calls == 0);
	state.finish(::war_test::completion::value);
	CHECK(result.calls == 1);
	CHECK(result.done == ::war_test::completion::stopped);
}

TEST_CASE("child stopped cancels siblings without stopping the outer source")
{
	auto states = ::std::array<::war_test::child_state, 2>{};
	states[1].stop_on_request = true;
	auto source = ::stdexec::inplace_stop_source{};
	auto children = states | ::std::views::transform([](auto&& state) {
		return ::war_test::controlled_sender{ state };
	});
	auto result = ::war_test::result{};
	auto operation = ::stdexec::connect(::nc::when_all_range(children),
		::war_test::receiver{ result, source.get_token() });
	::stdexec::start(operation);
	states[0].finish(::war_test::completion::stopped);
	CHECK(states[1].completions == 1);
	CHECK(states[1].token.stop_requested());
	CHECK_FALSE(source.stop_requested());
	CHECK(result.calls == 1);
	CHECK(result.done == ::war_test::completion::stopped);
}

TEST_CASE("a child error overrides outer cancellation")
{
	auto state = ::war_test::child_state{};
	state.error = ::std::make_exception_ptr(::std::runtime_error{ "failure after stop" });
	auto source = ::stdexec::inplace_stop_source{};
	auto result = ::war_test::result{};
	auto children = ::std::array{ ::war_test::controlled_sender{ state } };
	auto operation = ::stdexec::connect(::nc::when_all_range(children),
		::war_test::receiver{ result, source.get_token() });
	::stdexec::start(operation);
	source.request_stop();
	state.finish(::war_test::completion::error);
	CHECK(result.calls == 1);
	CHECK(result.done == ::war_test::completion::error);
	CHECK(result.error == state.error);
}

TEST_CASE("outer stop callback is removed after successful completion")
{
	auto source = ::stdexec::inplace_stop_source{};
	auto result = ::war_test::result{};
	{
		auto operation = ::stdexec::connect(::nc::when_all_range(::std::array{ ::stdexec::just() }),
			::war_test::receiver{ result, source.get_token() });
		::stdexec::start(operation);
		CHECK(result.calls == 1);
	}
	source.request_stop();
	CHECK(result.calls == 1);
	CHECK(result.done == ::war_test::completion::value);
}

TEST_CASE("sync_wait maps child stopped to an empty optional")
{
	auto children = ::std::array{ ::stdexec::just_stopped() };
	CHECK_FALSE(::stdexec::sync_wait(::nc::when_all_range(children)).has_value());
}
