#pragma once

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

namespace promises
{
	struct terminate_on_unhandled_stopped
	{
		[[nodiscard]] static ::std::coroutine_handle<> unhandled_stopped() noexcept
		{
			::std::terminate();
		}
	};
}

NAGISA_BUILD_LIB_DETAIL_END
