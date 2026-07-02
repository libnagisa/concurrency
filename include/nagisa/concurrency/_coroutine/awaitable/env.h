#pragma once

/// @file env.h
/// @brief @ref environment — produce the suspending coroutine's stdexec
///        environment as the @c co_await result.

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

/// @brief Awaitable whose @c await_resume yields the parent coroutine's env.
///
/// Used inside a coroutine to grab the surrounding stdexec environment
/// in one expression. Falls back to an empty @c stdexec::env() when
/// not consumed through @c as_awaitable.
///
/// @code
///   auto env = co_await environment();
///   auto sched = stdexec::get_scheduler(env);
/// @endcode
struct environment_t : ::std::suspend_never
{
	constexpr static auto await_resume() noexcept { return ::stdexec::env(); }

	constexpr auto as_awaitable(/*::stdexec::environment_provider*/ auto& parent) const noexcept
	{
		struct awaitable : ::std::suspend_never
		{
			constexpr decltype(auto) await_resume() const noexcept { return ::stdexec::get_env(*_parent); }

			::std::remove_reference_t<decltype(parent)>* _parent;
		};
		return awaitable{ {}, ::std::addressof(parent) };
	}
};

/// @brief Factory for @ref environment_t.
constexpr auto environment() noexcept
{
	return environment_t{};
}

NAGISA_BUILD_LIB_DETAIL_END
