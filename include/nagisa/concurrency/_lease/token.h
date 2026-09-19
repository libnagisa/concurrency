#pragma once

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

template <class T>
concept lease_token =
	::std::default_initializable<T> &&
	::std::movable<T> &&
	::std::is_nothrow_move_constructible_v<T> &&
	::std::is_nothrow_move_assignable_v<T>;

NAGISA_BUILD_LIB_DETAIL_END