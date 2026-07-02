#pragma once

/// @file with_awaitable.h
/// @brief Promise mixin that hooks @c await_transform into stdexec's
///        @c as_awaitable, letting senders be @c co_await'ed directly.

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

namespace promises
{
#if NAGISA_CONCURRENCY_USE_EXECUTION
	/// @brief Promise mixin: route any @c co_await expression through
	///        @c stdexec::as_awaitable, with @c Derived as the receiver context.
	///
	/// CRTP-style: pass the final promise type as @p Derived so
	/// @c as_awaitable can query the promise's env, see what kind of
	/// receiver it is, etc.
	///
	/// This is what makes statements like
	/// @code
	///   co_await stdexec::schedule(my_sched);
	///   co_await some_sender;
	/// @endcode
	/// work inside a coroutine of this type — without this mixin, the
	/// sender wouldn't satisfy @c awaitable.
	///
	/// @tparam Derived The final promise type.
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
