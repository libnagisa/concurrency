#pragma once

#include "./awaitable/forward.h"
#include "./awaitable/from_handle.h"
#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

template<class Handle>
struct current_handle_impl : from_handle<Handle>
{
	using base_type = from_handle<Handle>;

	constexpr auto as_awaitable(auto&& parent) const noexcept
	{
		using promise_type = ::std::remove_cvref_t<decltype(parent)>;

		return forward<::std::coroutine_handle<promise_type>>(
			::std::coroutine_handle<promise_type>::from_promise(parent)
		);
	}
};

template<class Handle = ::std::coroutine_handle<>>
constexpr auto current_handle() noexcept
{
	return current_handle_impl<Handle>{};
}

NAGISA_BUILD_LIB_DETAIL_END