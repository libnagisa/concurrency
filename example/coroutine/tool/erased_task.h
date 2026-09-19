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
    , nc::promises::with_await_transform<erased_promise>
    , nc::promises::with_scheduler<::std::optional<nc::any_scheduler>>
    , nc::promises::with_stop_token<>
{
    constexpr auto get_env() const noexcept
    {
        return ::stdexec::env(nc::promises::with_scheduler<::std::optional<nc::any_scheduler>>::get_env(), nc::promises::with_stop_token<>::get_env());
    }

    constexpr auto unhandled_stopped() const noexcept
    {
        return continuation();
    }
};