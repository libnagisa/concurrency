#pragma once

/// @file concurrency.h
/// @brief Convenience header that includes the entire public API of
///        @c nagisa.concurrency.
///
/// Includes the coroutine component library, @ref simple_task,
/// @ref spawn, and @ref any_scheduler. Pull this header in for normal
/// application code; the smaller headers are for fine-grained
/// dependency control inside the library.

#include <nagisa/concurrency/coroutine.h>
#include <nagisa/concurrency/simple_task.h>
#include <nagisa/concurrency/any_scheduler.h>