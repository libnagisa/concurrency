#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <array>
#include <concepts>

#include <doctest/doctest.h>

#include "support.h"

TEST_CASE("connected operations are immovable and cleaned up without start")
{
	auto states = ::std::array<::war_test::child_state, 3>{};
	auto result = ::war_test::result{};
	{
		auto children = states | ::std::views::transform([](auto&& state) {
			return ::war_test::controlled_sender{ state };
		});
		auto operation = ::stdexec::connect(::nc::when_all_range(children), ::war_test::receiver{ result });
		static_assert(!::std::move_constructible<decltype(operation)>);
		static_assert(!::std::copy_constructible<decltype(operation)>);
		for (auto&& state : states)
		{
			CHECK(state.connects == 1);
			CHECK(state.alive == 1);
			CHECK(state.starts == 0);
		}
	}
	for (auto&& state : states)
	{
		CHECK(state.alive == 0);
		CHECK(state.destroyed == 1);
	}
	CHECK(result.calls == 0);
}

TEST_CASE("throwing connect destroys exactly the already constructed operations")
{
	for (auto failing_index : { 0, 1, 2 })
	{
		CAPTURE(failing_index);
		auto states = ::std::array<::war_test::child_state, 3>{};
		states[failing_index].throw_on_connect = true;
		auto children = states | ::std::views::transform([](auto&& state) {
			return ::war_test::controlled_sender{ state };
		});
		auto result = ::war_test::result{};
		CHECK_THROWS_WITH_AS(::stdexec::connect(::nc::when_all_range(children), ::war_test::receiver{ result }),
			"child connect failed", ::std::runtime_error);
		for (auto index = 0; index != 3; ++index)
		{
			CHECK(states[index].connects == (index <= failing_index ? 1 : 0));
			CHECK(states[index].destroyed == (index < failing_index ? 1 : 0));
			CHECK(states[index].alive == 0);
			CHECK(states[index].starts == 0);
		}
		CHECK(result.calls == 0);
	}
}

TEST_CASE("throwing lazy range evaluation cleans up earlier connections")
{
	auto states = ::std::array<::war_test::child_state, 3>{};
	auto children = ::std::views::iota(0, 3) | ::std::views::transform([&](int index) {
		if (index == 2)
			throw ::std::runtime_error{ "range evaluation failed" };
		return ::war_test::controlled_sender{ states[index] };
	});
	auto result = ::war_test::result{};
	CHECK_THROWS_WITH_AS(::stdexec::connect(::nc::when_all_range(children), ::war_test::receiver{ result }),
		"range evaluation failed", ::std::runtime_error);
	CHECK(states[0].destroyed == 1);
	CHECK(states[1].destroyed == 1);
	CHECK(states[2].connects == 0);
	for (auto&& state : states)
	{
		CHECK(state.alive == 0);
		CHECK(state.starts == 0);
	}
	CHECK(result.calls == 0);
}

TEST_CASE("child operations survive completion until the parent operation is destroyed")
{
	auto state = ::war_test::child_state{};
	auto result = ::war_test::result{};
	{
		auto children = ::std::array{ ::war_test::controlled_sender{ state } };
		auto operation = ::stdexec::connect(::nc::when_all_range(children), ::war_test::receiver{ result });
		::stdexec::start(operation);
		state.finish(::war_test::completion::value);
		CHECK(result.calls == 1);
		CHECK(state.alive == 1);
		CHECK(state.destroyed == 0);
	}
	CHECK(state.alive == 0);
	CHECK(state.destroyed == 1);
}
