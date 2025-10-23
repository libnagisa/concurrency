#pragma once

#include "./awaitable.h"
#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

template<class T>
concept promise = ::std::same_as<T, ::std::remove_reference_t<T>>&& requires(T& promise)
{
	{ promise.initial_suspend() } -> awaitable;
	{ promise.final_suspend() } -> awaitable;
	{ promise.get_return_object() };
	{ promise.unhandled_exception() };
	// requires (requires{ promise.return_value()})
};

NAGISA_BUILD_LIB_DETAIL_END