#pragma once

#include <optional>
#include <iostream>
#include <nagisa/concurrency/concurrency.h>

namespace nc = ::nagisa::concurrency;
struct erased_promise;

template<class Promise, class Parent>
using erased_awaitable = nc::build_awaitable_t<Promise, Parent
    , nc::awaitable_traits::ready_false
    , nc::awaitable_traits::this_then_parent
    , nc::awaitable_traits::run_this
    , nc::awaitable_traits::rethrow_exception
    , nc::awaitable_traits::destroy_after_resumed
>;

using erased_task = nc::basic_task<erased_promise, erased_awaitable>;

struct erased_promise
    : nc::promises::lazy
    , nc::promises::return_object_from_handle<erased_promise, erased_task>
    , nc::promises::value<void>
    , nc::promises::exception<true>
    , nc::promises::jump_to_continuation<>
    , nc::promises::without_stop_token
    , nc::promises::with_await_transform<erased_promise>
    , nc::promises::with_scheduler<::std::optional<nc::any_scheduler>>
{
};

struct scheduler
{
	constexpr explicit(false) scheduler(::std::size_t const& id) noexcept : _id(::std::addressof(id)) {}
	struct schedule_t : ::std::suspend_never
	{
		auto await_resume() const noexcept
		{
			::std::cout << "scheduler[" << *_id << "] scheduling coroutine on thread " << std::this_thread::get_id() << '\n';
		}
		constexpr auto get_env() const noexcept { return *this; }
		template<class Tag> constexpr auto query(::stdexec::get_completion_scheduler_t<Tag>) const { return scheduler{ *_id }; }
		::std::size_t const* _id;
	};
	constexpr auto schedule() noexcept { return schedule_t{ {}, _id }; }
	constexpr bool operator==(scheduler const&) const noexcept = default;
	::std::size_t const* _id = nullptr;
};
static_assert(::stdexec::scheduler<scheduler>);