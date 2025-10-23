#pragma once

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

namespace promises
{
#if NAGISA_CONCURRENCY_USE_EXECUTION
	struct stoppable
	{
		struct env_type
		{

			constexpr auto&& query(::stdexec::get_stop_token_t) const noexcept
			{
				return _self->_stop_token;
			}
			stoppable const* _self;
		};

		constexpr auto get_env() const noexcept { return env_type{ this }; }

		[[nodiscard]] auto stop_requested() const noexcept
		{
			return _stop_token.stop_requested();
		}

		::stdexec::inplace_stop_token _stop_token;
	};

#endif
}

NAGISA_BUILD_LIB_DETAIL_END