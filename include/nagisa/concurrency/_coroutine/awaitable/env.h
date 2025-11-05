#pragma once

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

struct environment_t : ::std::suspend_never
{
	constexpr static auto await_resume() noexcept { return ::stdexec::env(); }

	constexpr auto as_awaitable(::stdexec::environment_provider auto& parent) const noexcept
	{
		struct awaitable : ::std::suspend_never
		{
			constexpr decltype(auto) await_resume() const noexcept { return ::stdexec::get_env(*_parent); }

			::std::remove_reference_t<decltype(parent)>* _parent;
		};
		return awaitable{ {}, ::std::addressof(parent) };
	}
};

constexpr auto environment() noexcept
{
	return environment_t{};
}

NAGISA_BUILD_LIB_DETAIL_END