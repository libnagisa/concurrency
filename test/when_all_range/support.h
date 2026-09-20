#pragma once

#include <atomic>
#include <cassert>
#include <exception>
#include <optional>
#include <stdexcept>
#include <utility>

#include <stdexec/execution.hpp>

#include <nagisa/concurrency/when_all_range.h>

namespace nc = ::nagisa::concurrency;

namespace war_test
{
enum class completion
{
	pending,
	value,
	error,
	stopped
};

struct result
{
	::std::atomic<int> calls{ 0 };
	::std::atomic<completion> done{ completion::pending };
	::std::exception_ptr error{};
};

template <class StopToken = ::stdexec::never_stop_token>
struct receiver
{
	using receiver_concept = ::stdexec::receiver_t;

	result& output;
	StopToken token{};

	auto set_value() && noexcept -> void
	{
		output.done.store(completion::value);
		++output.calls;
	}

	auto set_error(::std::exception_ptr error) && noexcept -> void
	{
		output.error = ::std::move(error);
		output.done.store(completion::error);
		++output.calls;
	}

	auto set_stopped() && noexcept -> void
	{
		output.done.store(completion::stopped);
		++output.calls;
	}

	[[nodiscard]] auto get_env() const noexcept
	{
		return ::stdexec::prop{ ::stdexec::get_stop_token, token };
	}
};

// Each child can be completed explicitly, without timing assumptions or sleeps.
// Its storage outlives the connected operation and is touched by one completing thread.
struct child_state
{
	completion on_start = completion::pending;
	bool stop_on_request = false;
	bool throw_on_connect = false;
	::std::exception_ptr error{};
	int connects = 0;
	int starts = 0;
	int completions = 0;
	int alive = 0;
	int destroyed = 0;
	::stdexec::inplace_stop_token token{};
	void* operation = nullptr;
	void (*complete)(void*, completion) noexcept = nullptr;

	auto finish(completion kind) -> void
	{
		assert(operation != nullptr);
		complete(operation, kind);
	}
};

struct controlled_sender
{
	using sender_concept = ::stdexec::sender_t;
	using completion_signatures = ::stdexec::completion_signatures<
		::stdexec::set_value_t(),
		::stdexec::set_error_t(::std::exception_ptr),
		::stdexec::set_stopped_t()>;

	child_state& state;

	explicit controlled_sender(child_state& source) noexcept : state(source) {}
	controlled_sender(controlled_sender&&) = default;
	controlled_sender(controlled_sender const&) = delete;

	template <class Receiver>
	struct operation
	{
		using operation_state_concept = ::stdexec::operation_state_t;

		struct on_stop
		{
			operation* owner;
			auto operator()() const noexcept -> void { owner->finish(completion::stopped); }
		};

		child_state& state;
		Receiver receiver;
		::std::optional<::stdexec::inplace_stop_callback<on_stop>> callback{};

		operation(child_state& source, Receiver target) noexcept
			: state(source), receiver(::std::move(target))
		{
			++state.alive;
		}
		operation(operation&&) = delete;
		~operation()
		{
			--state.alive;
			++state.destroyed;
		}

		auto finish(completion kind) noexcept -> void
		{
			assert(state.operation == this);
			state.operation = nullptr;
			++state.completions;
			callback.reset();
			switch (kind)
			{
			case completion::value:
				::stdexec::set_value(::std::move(receiver));
				return;
			case completion::error:
				::stdexec::set_error(::std::move(receiver), state.error);
				return;
			case completion::stopped:
				::stdexec::set_stopped(::std::move(receiver));
				return;
			case completion::pending:
				::std::terminate();
			}
		}

		auto start() & noexcept -> void
		{
			++state.starts;
			state.token = ::stdexec::get_stop_token(::stdexec::get_env(receiver));
			state.operation = this;
			state.complete = [](void* self, completion kind) noexcept {
				static_cast<operation*>(self)->finish(kind);
			};
			if (state.on_start != completion::pending)
				finish(state.on_start);
			else if (state.stop_on_request)
				callback.emplace(state.token, on_stop{ this });
		}
	};

	template <::stdexec::receiver Receiver>
	[[nodiscard]] auto connect(Receiver target) && -> operation<Receiver>
	{
		++state.connects;
		if (state.throw_on_connect)
			throw ::std::runtime_error{ "child connect failed" };
		return operation<Receiver>{ state, ::std::move(target) };
	}
};
}
