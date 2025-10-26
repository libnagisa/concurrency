#include <print>
#include <nagisa/concurrency/concurrency.h>
#include <ranges>
#include <algorithm>
#include <cassert>
#include <any>
#include <stdexec/execution.hpp>
#include <stdexec/coroutine.hpp>

namespace nc = ::nagisa::concurrency;
namespace nat = ::nc::awaitable_traits;

struct promise;

template<class Promise, class Parent>
using task_awaitable_trait = ::nc::awaitable_trait_combiner_t<Promise, Parent,
	::nat::ready_if_done

	, ::nat::capture_scheduler
	, ::nat::capture_stop_token
	, ::nat::this_then_parent
	, ::nat::run_this

	, ::nat::release_value
	, ::nat::rethrow_exception
	, ::nat::destroy_after_resumed
>;

template<class Promise, class Parent>
using trait_instance = ::nc::awaitable_trait_instance_t<task_awaitable_trait, Promise, Parent>;

struct forward_stop_request {
	::stdexec::inplace_stop_source& _stop_source;

	void operator()() const noexcept {
		_stop_source.request_stop();
	}
};

template<class Promise, class Parent>
struct stop_token_holder
{};


struct context_type
{
	::stdexec::inplace_stop_source stop_source{};
	::std::any stop_callback{};
};

template<class Promise, class Parent>
	requires !requires(::std::coroutine_handle<Promise> handle, ::std::coroutine_handle<Parent> parent)
	{
		::nat::capture_stop_token<Promise, Parent>::await_suspend(handle, parent);
	} && requires{ ::nc::set_stop_token(self.promise(), ::stdexec::get_stop_token(::stdexec::get_env(parent.promise()))); }
struct stop_token_holder<Promise, Parent>
{
	using stop_token_t = ::stdexec::stop_token_of_t<::stdexec::env_of_t<Parent>>;
	using callback_t = ::stdexec::stop_callback_for_t<stop_token_t, forward_stop_request>;

	struct context_type
	{
		::stdexec::inplace_stop_source stop_source{};
		::std::any stop_callback{};
	};
	static auto create_context()

	template<class Parent>
	stop_token_holder(::std::coroutine_handle<Promise> handle, Parent& promise)	noexcept
	{
		if constexpr (!requires{ ::nat::capture_stop_token<Promise, Parent>::await_suspend(handle, ::std::coroutine_handle<Parent>::from_promise(promise)); })
		{
			if (auto token = ::stdexec::get_stop_token(::stdexec::get_env(promise)); token.stop_possible())
			{

				_stop_callback.emplace<callback_t>(::std::move(token), forward_stop_request{ _stop_source });
				::nc::set_stop_token(handle.promise(), _stop_source.get_token());
			}
		}
	}
	::stdexec::inplace_stop_source _stop_source{};
	::std::any _stop_callback{};
};

template<class Promise>
struct stop_token_holder<Promise, void>
{
	template<class Parent>
	constexpr static auto create_context(::std::coroutine_handle<Promise> handle, Parent& promise) noexcept
	{
		context_type result{};
		if constexpr(!requires{ ::nat::capture_stop_token<Promise, Parent>::await_suspend(handle, ::std::coroutine_handle<Parent>::from_promise(promise)); })
		{
			if (auto token = ::stdexec::get_stop_token(::stdexec::get_env(promise)); token.stop_possible())
			{
				using stop_token_t = decltype(token);
				using callback_t = ::stdexec::stop_callback_for_t<stop_token_t, forward_stop_request>;
				result.stop_callback.emplace<callback_t>(::std::move(token), forward_stop_request{ result.stop_source });
				::nc::set_stop_token(handle.promise(), result.stop_source.get_token());
			}
		}
		return result;
	}

};
template<class Promise, class Parent>
	requires requires(::std::coroutine_handle<Promise> handle, ::std::coroutine_handle<Parent> parent)
	{
		::nat::capture_stop_token<Promise, Parent>::await_suspend(handle, parent);
	}
struct stop_token_holder<Promise, Parent>{};



using task = ::nc::basic_task<promise, task_awaitable>;

struct promise
	: ::nc::promises::lazy
	, ::nc::promises::exception<true>
	, ::nc::promises::value<void>
	, ::nc::promises::jump_to_continuation<>
	, ::nc::promises::return_object_from_handle<promise, task>
	, ::nc::promises::with_scheduler<>
	, ::nc::promises::with_stop_token<>
	, ::nc::promises::use_as_awaitable<promise>
{
	constexpr auto get_env() const noexcept
	{
		return ::stdexec::env(::nc::promises::with_scheduler<>::get_env(), ::nc::promises::with_stop_token<>::get_env());
	}
};

static_assert(::nc::awaitable<task_awaitable<promise, void>>);
static_assert(::nc::awaitable<task>);

struct get_current_handle_t
{
	auto await_ready() const noexcept { return false; }
	::std::coroutine_handle<> result;
	auto await_suspend(::std::coroutine_handle<> parent) noexcept { result = parent;  return parent; }
	auto await_resume() const noexcept { return result; }
};
struct check_stop_t
{
	auto await_ready() const noexcept { return false; }
	template<class Promise>
	::std::coroutine_handle<> await_suspend(::std::coroutine_handle<Promise> parent) noexcept
	{
		if constexpr (requires { ::stdexec::get_stop_token(::stdexec::get_env(parent.promise())); })
		{
			static_assert(requires{ { parent.promise().unhandled_stopped() }; });
			if (::stdexec::get_stop_token(::stdexec::get_env(parent.promise())).stop_requested())
				return parent.promise().unhandled_stopped();
		}
		return parent;
	}
	auto await_resume() const noexcept { }
};

task f1(int i) noexcept
{
	::std::println("{}", i);
	co_await check_stop_t{};
	auto&& sche = co_await ::stdexec::get_scheduler();
	co_await sche.schedule();
	co_await ::stdexec::just_stopped();
	if (!i)
		co_return;
	co_await f1(i - 1);
}



int main()
{
	auto task = f1(5);
	task._coroutine.resume();
	assert(task._coroutine.done());

	return 0;
}