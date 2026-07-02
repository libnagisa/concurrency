// #pragma once

/// @file environment.h
/// @brief Bootstrap macros for the @c nagisa.concurrency library namespace.
///
/// Defines @c NAGISA_BUILD_LIB_NAME / @c NAGISA_BUILD_LIB_CONFIG_VERSION
/// and then delegates the actual macro construction to
/// @c nagisa/build_lib/construct.h. Each translation unit including
/// this file gets the @c NAGISA_BUILD_LIB_BEGIN / @c _END /
/// @c _DETAIL_BEGIN / @c _DETAIL_END scope macros it needs.

#define NAGISA_BUILD_LIB_NAME NAGISA_NS::concurrency
#define NAGISA_BUILD_LIB_CONFIG_VERSION (0,0,0)
#include <nagisa/build_lib/construct.h>
