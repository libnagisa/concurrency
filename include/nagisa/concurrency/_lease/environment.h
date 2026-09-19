#pragma once

#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <concepts>
#include <utility>
#include <memory>
#include <cassert>
#include <optional>
#include <array>
#include <ranges>

#include <mutex>

#if !__has_include(<stdexec/execution.hpp>)
#	error "the nagisa.concurrency library requires a C++20 compiler with support for the stdexec library"
#endif

#include <stdexec/execution.hpp>

#include <nagisa/concurrency/_forward_stop_token.h>

#include <nagisa/concurrency/environment.h>