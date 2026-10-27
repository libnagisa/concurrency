#pragma once

#include "./basic_semaphore.h"

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

template <::std::ptrdiff_t LeastMaxValue = (::std::numeric_limits<::std::ptrdiff_t>::max)()>
using counting_semaphore = basic_semaphore<LeastMaxValue>;

NAGISA_BUILD_LIB_DETAIL_END
