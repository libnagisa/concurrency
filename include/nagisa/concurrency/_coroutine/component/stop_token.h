#pragma once

#include "./continuation.h"
#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

inline constexpr struct set_stop_token_t
{
	enum class _requires_result
	{
		member,
		tag_invoke,
		none,
	};
	template<class Promise, class Token>
	consteval static auto _check() noexcept
	{
		if constexpr (requires(Promise & promise, Token token) { promise.set_stop_token(::std::forward<decltype(token)>(token)); })
			return _requires_result::member;
		else if constexpr (requires(Promise & promise, Token token) { tag_invoke(set_stop_token_t{}, promise, ::std::forward<decltype(token)>(token)); })
			return _requires_result::tag_invoke;
		return _requires_result::none;
	}
	constexpr decltype(auto) operator()(auto& promise, auto&& token) const noexcept
		requires (_check<decltype(promise)>() != _requires_result::none)
	{
		constexpr auto result = _check<decltype(promise), decltype(token)>();
		if constexpr (result == _requires_result::member)
			return promise.set_stop_token(::std::forward<decltype(token)>(token));
		else if constexpr (result == _requires_result::tag_invoke)
			return tag_invoke(set_stop_token_t{}, promise, ::std::forward<decltype(token)>(token));
	}
} set_stop_token{};

namespace awaitable_traits
{
#if NAGISA_CONCURRENCY_USE_EXECUTION

	template<class Promise, class ParentPromise>
	struct capture_stop_token
	{
		using promise_type = Promise;
		using handle_type = ::std::coroutine_handle<promise_type>;

		constexpr static decltype(auto) await_suspend(handle_type self, ::std::coroutine_handle<ParentPromise> parent) noexcept
			requires requires{ set_stop_token(self.promise(), ::stdexec::get_stop_token(::stdexec::get_env(parent.promise()))); }
		{
			set_stop_token(self.promise(), ::stdexec::get_scheduler(::stdexec::get_env(parent.promise())));
		}
	};

#endif
}

namespace promises
{
#if NAGISA_CONCURRENCY_USE_EXECUTION

	struct stop_token
	{
		using self_type = stop_token;
		using stop_callback_t = ::std::coroutine_handle<>(*)(void*) noexcept;

		struct env_type
		{

			constexpr auto&& query(::stdexec::get_stop_token_t) const noexcept
			{
				return _self->_stop_token;
			}
			self_type const* _self;
		};

		constexpr auto get_env() const noexcept { return env_type{ this }; }

		[[nodiscard]] auto stop_requested() const noexcept
		{
			return _stop_token.stop_requested();
		}
		[[nodiscard]] static ::std::coroutine_handle<> unhandled_stopped() noexcept
		{
			::std::terminate();
		}
		auto set_stop_token(auto&& token) noexcept
			requires ::std::assignable_from<::stdexec::inplace_stop_token&, decltype(token)>
		{
			_stop_token = ::std::forward<decltype(token)>(token);
		}
		::stdexec::inplace_stop_token _stop_token{};
	};

#endif
}

NAGISA_BUILD_LIB_DETAIL_END