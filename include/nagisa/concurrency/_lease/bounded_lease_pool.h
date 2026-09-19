#pragma once

#include "./bounded_stack.h"
#include "./lease_pool.h"
#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

template<class Token, ::std::size_t N>
using bounded_lease_pool = lease_pool<Token, bounded_stack<Token, N>>;

NAGISA_BUILD_LIB_DETAIL_END