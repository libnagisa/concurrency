#pragma once

#include <nagisa/concurrency/lease.h>
#include <stdexec/execution.hpp>

#include <coroutine>
#include <exception>
#include <optional>
#include <utility>
#include <vector>

namespace nc = ::nagisa::concurrency;

enum class completion
{
	pending,
	value,
	stopped
};

template <class Lease, class StopToken = ::stdexec::never_stop_token>
struct capture_receiver
{
	using receiver_concept = ::stdexec::receiver_t;

	::std::optional<Lease>* out = nullptr;
	completion* done = nullptr;
	StopToken token{};

	void set_value(Lease lease) noexcept
	{
		*out = ::std::move(lease);
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

template <class T>
struct await_task
{
	struct promise_type
	{
		::std::optional<T> value{};
		await_task get_return_object() noexcept
		{
			return await_task{::std::coroutine_handle<promise_type>::from_promise(*this)};
		}
		::std::suspend_always initial_suspend() noexcept { return {}; }
		::std::suspend_always final_suspend() noexcept { return {}; }
		void return_value(T v) noexcept { value = ::std::move(v); }
		void unhandled_exception() noexcept { ::std::terminate(); }
		::std::coroutine_handle<> unhandled_stopped() noexcept { return ::std::noop_coroutine(); }
		auto await_transform(auto&& sndr)
		{
			return ::stdexec::as_awaitable(::std::forward<decltype(sndr)>(sndr), *this);
		}
		auto get_env() const noexcept
		{
			return ::stdexec::prop{::stdexec::get_stop_token, ::stdexec::never_stop_token{}};
		}
	};

	::std::coroutine_handle<promise_type> handle{};
	explicit await_task(::std::coroutine_handle<promise_type> h) noexcept : handle(h) {}
	await_task(await_task&& other) noexcept : handle(::std::exchange(other.handle, {})) {}
	~await_task()
	{
		if (handle)
			handle.destroy();
	}
	void resume() const { handle.resume(); }
	[[nodiscard]] bool done() const noexcept { return handle.done(); }
	[[nodiscard]] T take()
	{
		return *handle.promise().value;
	}
};

using int_pool = ::nc::lease_pool<int, ::std::vector<int>>;
using int_lease = int_pool::lease;
