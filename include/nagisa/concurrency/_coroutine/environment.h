#pragma once

#include <concepts>
#include <coroutine>
#include <utility>
#include <cstdlib>
#include <cstdint>
#include <cstddef>
#include <optional>
#include <exception>
#include <type_traits>
#include <tuple>
#include <any>

#if !defined(NAGISA_CONCURRENCY_USE_EXECUTION)
#	if __has_include(<stdexec/execution.hpp>)
#		define NAGISA_CONCURRENCY_USE_EXECUTION 1
#	else
#		define NAGISA_CONCURRENCY_USE_EXECUTION 0
#	endif
#endif

#if NAGISA_CONCURRENCY_USE_EXECUTION
#	include <stdexec/execution.hpp>
#endif

#include "../environment.h"