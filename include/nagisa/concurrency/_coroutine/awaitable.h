#pragma once

#include "./awaiter.h"
#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN


#if defined(_MSC_VER) && !defined(__clang__)
// MSVCBUG https://developercommunity.visualstudio.com/t/operator-co_await-not-found-in-requires/10452721

template <class Awaitable>
void co_await_constraint(Awaitable&& a) noexcept
	requires requires { operator co_await(static_cast<Awaitable&&>(a)); };
#endif

template <class Awaitable>
decltype(auto) get_awaiter(Awaitable&& a, auto const&)
{
	if constexpr (requires { static_cast<Awaitable&&>(a).operator co_await(); }) {
		return static_cast<Awaitable&&>(a).operator co_await();
	}
	else if constexpr (requires {
#if defined(_MSC_VER) && !defined(__clang__)
		co_await_constraint(static_cast<Awaitable&&>(a));
#else
		operator co_await(static_cast<Awaitable&&>(a));
#endif
	}) {
		return operator co_await(static_cast<Awaitable&&>(a));
	}
	else {
		return static_cast<Awaitable&&>(a);
	}
}

template <class Awaitable, class Promise>
decltype(auto) get_awaiter(Awaitable&& a, Promise* p)
	requires requires { p->await_transform(static_cast<Awaitable&&>(a)); }
{
	if constexpr (
		requires {
		p->await_transform(static_cast<Awaitable&&>(a)).operator co_await();
	}) {
		return p->await_transform(static_cast<Awaitable&&>(a)).operator co_await();
	}
	else if constexpr (requires {
#if defined(_MSC_VER) && !defined(__clang__)
		co_await_constraint(
			p->await_transform(static_cast<Awaitable&&>(a)));
#  else
		operator co_await(p->await_transform(static_cast<Awaitable&&>(a)));
#  endif
	}) {
		return operator co_await(p->await_transform(static_cast<Awaitable&&>(a)));
	}
	else {
		return p->await_transform(static_cast<Awaitable&&>(a));
	}
}

template <class Awaitable, class... Promise>
concept awaitable = requires(Awaitable && a, Promise*... p) {
	{ details::get_awaiter(static_cast<Awaitable&&>(a), p...) } -> awaiter<Promise...>;
};
template<class T, class... P> requires awaitable<T, P...>
using awaitable_awaiter_t = decltype(details::get_awaiter(::std::declval<T>(), ::std::declval<P*>()...));


NAGISA_BUILD_LIB_DETAIL_END