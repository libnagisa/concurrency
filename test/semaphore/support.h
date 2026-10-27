#pragma once

#include <nagisa/concurrency/semaphore.h>
#include <stdexec/execution.hpp>

#include <optional>
#include <utility>

namespace nc = ::nagisa::concurrency;

enum class completion
{
	pending,
	value,
	stopped,
	error
};

template <class StopToken = ::stdexec::never_stop_token>
struct capture_receiver
{
	using receiver_concept = ::stdexec::receiver_t;

	completion* done = nullptr;
	StopToken token{};

	void set_value() noexcept
	{
		*done = completion::value;
	}
	void set_error(::std::exception_ptr) noexcept
	{
		::std::terminate();
	}
	void set_stopped() noexcept
	{
		*done = completion::stopped;
	}
	[[nodiscard]] auto get_env() const noexcept
	{
		return ::stdexec::prop{::stdexec::get_stop_token, token};
	}
};
