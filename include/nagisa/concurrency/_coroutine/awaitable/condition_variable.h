#pragma once

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

template<class Queue>
struct condition_variable
{
	using queue_type = Queue;

	constexpr explicit(false) condition_variable(auto&&... args)
		noexcept(::std::is_nothrow_constructible_v<queue_type, decltype(args)...>)
		requires ::std::constructible_from<queue_type, decltype(args)...>
		: _queue(::std::forward<decltype(args)>(args)...)
	{}



	queue_type _queue;
};

NAGISA_BUILD_LIB_DETAIL_END