#pragma once

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

enum class completion_kind : ::std::uint8_t
{
	value,
	error,
	stopped,
};

NAGISA_BUILD_LIB_DETAIL_END