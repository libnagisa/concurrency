#pragma once

#include "./bounded_stack.h"
#include "./single_slot.h"
#include "./resource_pool.h"
#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

// 容量为 1 时用单槽容器：只需要一个标志位，不需要 size_t 和数组下标。
template <class Token, ::std::size_t N>
using bounded_container = ::std::conditional_t<N == 1, single_slot<Token>, bounded_stack<Token, N>>;

template <class Token, ::std::size_t N>
using bounded_resource_pool = resource_pool<Token, bounded_container<Token, N>>;

NAGISA_BUILD_LIB_DETAIL_END
