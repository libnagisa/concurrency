#pragma once

/// @file noop_promise.h
/// @brief A minimal "does-nothing" promise used as a default
///        @c ParentPromise placeholder.
///
/// @ref noop_promise inherits @c std::noop_coroutine_promise (so its
/// behaviors are well-defined) and adds the two mixins required to
/// participate in this library's plumbing:
///   - @c promises::without_stop_token — provides @c unhandled_stopped,
///   - @c promises::with_await_transform — routes @c co_await through
///     @c stdexec::as_awaitable.
///
/// Use it as a stand-in when you need *some* promise type but don't
/// care about its behavior — for instance, when type-erasing an
/// awaitable that should work regardless of the calling coroutine
/// (see @ref any_scheduler).

#include "./component/with_awaitable.h"
#include "./component/stop_token.h"
#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

/// @brief A "do-nothing" promise type, suitable as a default
///        @c ParentPromise placeholder in trait composition and
///        type-erasure scenarios.
struct noop_promise
	: ::std::noop_coroutine_promise
	, promises::without_stop_token
	, promises::with_await_transform<noop_promise>
{
};

NAGISA_BUILD_LIB_DETAIL_END
