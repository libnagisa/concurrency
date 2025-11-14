#pragma once

#include "./component/with_awaitable.h"
#include "./component/stop_token.h"
#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

struct noop_promise
	: ::std::noop_coroutine_promise
	, promises::without_stop_token
	, promises::with_await_transform<noop_promise>
{
};

NAGISA_BUILD_LIB_DETAIL_END