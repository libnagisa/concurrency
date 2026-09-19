#pragma once

#include "./token.h"

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

template <class C, class Token>
concept available_container =
	lease_token<Token> &&
	requires(C & c, C const& cc, Token t) {
	c.push_back(::std::move(t));
	{ c.back() } -> ::std::convertible_to<Token&>;
	c.pop_back();
	{ cc.empty() } -> ::std::convertible_to<bool>;
	{ cc.size() } -> ::std::convertible_to<::std::size_t>;
};

NAGISA_BUILD_LIB_DETAIL_END