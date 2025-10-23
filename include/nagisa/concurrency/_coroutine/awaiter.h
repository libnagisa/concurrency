#pragma once

#include "./coroutine_handle.h"
#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

template <class T>
concept await_suspend_result = ::std::same_as<T, void> || ::std::same_as<T, bool> || coroutine_handle<T>;

template <class Awaiter, class ParentPromise = void>
concept awaiter = requires(Awaiter & a, ::std::coroutine_handle<ParentPromise> h) {
	a.await_ready() ? 1 : 0;
	{ a.await_suspend(h) } -> await_suspend_result;
	a.await_resume();
};

template<class T, class... P> requires awaiter<T, P...>
using awaiter_result_t = decltype(::std::declval<T&>().await_resume());

NAGISA_BUILD_LIB_DETAIL_END