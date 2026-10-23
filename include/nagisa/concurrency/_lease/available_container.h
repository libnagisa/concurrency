#pragma once

#include "./token.h"

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

template <class C, class Token>
concept available_container =
	lease_token<Token> &&
	requires(C & c, C const& cc, Token t) {
		c.push_back(::std::move(t));
		{ ::std::move(c.back()) } -> ::std::convertible_to<Token>;
		c.back() = ::std::move(t);
		c.pop_back();
		{ ::std::ranges::empty(cc) } -> ::std::convertible_to<bool>;
	};

NAGISA_BUILD_LIB_DETAIL_END