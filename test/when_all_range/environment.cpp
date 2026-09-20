#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <array>

#include <doctest/doctest.h>

#include "support.h"

namespace war_test
{
struct scheduler_receiver : receiver<::stdexec::inplace_stop_token>
{
	::stdexec::run_loop::scheduler scheduler;

	[[nodiscard]] auto get_env() const noexcept
	{
		return ::stdexec::env{
			::stdexec::prop{ ::stdexec::get_scheduler, scheduler },
			::stdexec::prop{ ::stdexec::get_stop_token, token }
		};
	}
};
}

TEST_CASE("children share an internal stop token distinct from the outer token")
{
	auto states = ::std::array<::war_test::child_state, 2>{};
	auto children = states | ::std::views::transform([](auto&& state) {
		return ::war_test::controlled_sender{ state };
	});
	auto source = ::stdexec::inplace_stop_source{};
	auto result = ::war_test::result{};
	auto operation = ::stdexec::connect(::nc::when_all_range(children),
		::war_test::receiver{ result, source.get_token() });
	::stdexec::start(operation);
	CHECK(states[0].token.stop_possible());
	CHECK(states[0].token == states[1].token);
	CHECK(states[0].token != source.get_token());
	states[0].finish(::war_test::completion::value);
	states[1].finish(::war_test::completion::value);
	CHECK(result.calls == 1);
}

TEST_CASE("outer scheduler is forwarded to environment-dependent children")
{
	auto loop = ::stdexec::run_loop{};
	auto scheduler = loop.get_scheduler();
	auto source = ::stdexec::inplace_stop_source{};
	auto observed = 0;
	auto make_child = [&] {
		return ::stdexec::get_scheduler() | ::stdexec::then([&](auto actual) {
			CHECK(actual == scheduler);
			++observed;
		});
	};
	auto children = ::std::array{ make_child(), make_child() };
	auto result = ::war_test::result{};
	auto receiver = ::war_test::scheduler_receiver{
		{ result, source.get_token() }, scheduler
	};
	auto operation = ::stdexec::connect(::nc::when_all_range(children), ::std::move(receiver));
	::stdexec::start(operation);
	CHECK(observed == 2);
	CHECK(result.calls == 1);
	CHECK(result.done == ::war_test::completion::value);
}
