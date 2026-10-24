#pragma once

#include "./basic_semaphore.h"

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

template <::std::ptrdiff_t Max = (::std::numeric_limits<::std::ptrdiff_t>::max)()>
using counting_semaphore = basic_semaphore<Max>;

NAGISA_BUILD_LIB_DETAIL_END
