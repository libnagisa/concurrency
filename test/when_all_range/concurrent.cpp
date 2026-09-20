#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <array>
#include <barrier>
#include <thread>
#include <vector>

#include <doctest/doctest.h>

#include "support.h"

TEST_CASE("simultaneous child completions publish every result exactly once")
{
	constexpr auto child_count = 8;
	auto states = ::std::array<::war_test::child_state, child_count>{};
	auto values = ::std::array<int, child_count>{};
	auto children = states | ::std::views::transform([](auto&& state) {
		return ::war_test::controlled_sender{ state };
	});
	auto sum = 0;
	auto sender = ::nc::when_all_range(children) | ::stdexec::then([&] {
		// Non-atomic child writes must be visible when the aggregate completes.
		for (auto value : values)
			sum += value;
	});
	auto result = ::war_test::result{};
	auto operation = ::stdexec::connect(::std::move(sender), ::war_test::receiver{ result });
	::stdexec::start(operation);
	auto gate = ::std::barrier{ child_count };
	{
		auto threads = ::std::vector<::std::jthread>{};
		threads.reserve(child_count);
		for (auto index = 0; index != child_count; ++index)
		{
			threads.emplace_back([&, index] {
				gate.arrive_and_wait();
				values[index] = index + 1;
				states[index].finish(::war_test::completion::value);
			});
		}
	}
	CHECK(sum == child_count * (child_count + 1) / 2);
	CHECK(result.calls == 1);
	CHECK(result.done == ::war_test::completion::value);
}

TEST_CASE("outer stop racing the last completion never completes twice")
{
	for (auto iteration = 0; iteration != 256; ++iteration)
	{
		CAPTURE(iteration);
		auto state = ::war_test::child_state{};
		auto source = ::stdexec::inplace_stop_source{};
		auto result = ::war_test::result{};
		auto children = ::std::array{ ::war_test::controlled_sender{ state } };
		auto operation = ::stdexec::connect(::nc::when_all_range(children),
			::war_test::receiver{ result, source.get_token() });
		::stdexec::start(operation);
		auto gate = ::std::barrier{ 2 };
		{
			auto completing = ::std::jthread{ [&] {
				gate.arrive_and_wait();
				state.finish(::war_test::completion::value);
			} };
			auto stopping = ::std::jthread{ [&] {
				gate.arrive_and_wait();
				source.request_stop();
			} };
		}
		CHECK(result.calls == 1);
		CHECK((result.done == ::war_test::completion::value || result.done == ::war_test::completion::stopped));
	}
}

TEST_CASE("concurrent errors report one of the original exceptions exactly once")
{
	auto states = ::std::array<::war_test::child_state, 4>{};
	for (auto&& state : states)
		state.error = ::std::make_exception_ptr(::std::runtime_error{ "concurrent failure" });
	auto children = states | ::std::views::transform([](auto&& state) {
		return ::war_test::controlled_sender{ state };
	});
	auto result = ::war_test::result{};
	auto operation = ::stdexec::connect(::nc::when_all_range(children), ::war_test::receiver{ result });
	::stdexec::start(operation);
	auto gate = ::std::barrier{ 4 };
	{
		auto threads = ::std::vector<::std::jthread>{};
		threads.reserve(states.size());
		for (auto index = ::std::size_t{ 0 }; index != states.size(); ++index)
		{
			threads.emplace_back([&, index] {
				gate.arrive_and_wait();
				states[index].finish(::war_test::completion::error);
			});
		}
	}
	CHECK(result.calls == 1);
	CHECK(result.done == ::war_test::completion::error);
	CHECK(::std::ranges::any_of(states, [&](auto&& state) { return state.error == result.error; }));
}
