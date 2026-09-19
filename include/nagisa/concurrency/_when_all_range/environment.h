#pragma once

#include <new>
#include <utility>
#include <span>
#include <ranges>
#include <algorithm>
#include <optional>
#include <memory>
#include <exception>
#include <type_traits>
#include <cstdint>
#include <cstddef>

#include <atomic>

#if !__has_include(<stdexec/execution.hpp>)
#	error "the nagisa.concurrency library requires a C++20 compiler with support for the stdexec library"
#endif

#include <stdexec/execution.hpp>

#include <nagisa/concurrency/_forward_stop_token.h>

#include <nagisa/concurrency/environment.h>