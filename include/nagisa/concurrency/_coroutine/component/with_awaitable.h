#pragma once

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

namespace promises
{
#if NAGISA_CONCURRENCY_USE_EXECUTION
	template<class Derived>
	struct with_await_transform
	{
		constexpr decltype(auto) await_transform(auto&& val) noexcept
		{
			static_assert(::std::derived_from<Derived, with_await_transform>);
			return ::stdexec::as_awaitable(::std::forward<decltype(val)>(val), static_cast<Derived&>(*this));
		}
	};
#endif
}

NAGISA_BUILD_LIB_DETAIL_END