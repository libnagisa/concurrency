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
		if constexpr (requires(Promise promise, Token token) { promise.set_stop_token(::std::forward<decltype(token)>(token)); })
			return _requires_result::member;
		else if constexpr (requires(Promise promise, Token token) { tag_invoke(set_stop_token_t{}, promise, ::std::forward<decltype(token)>(token)); })
			return _requires_result::tag_invoke;
		return _requires_result::none;
	}
	constexpr decltype(auto) operator()(auto& promise, auto&& token) const noexcept
		requires (_check<decltype(promise), decltype(token)>() != _requires_result::none)
	{
		constexpr auto result = _check<decltype(promise), decltype(token)>();
		if constexpr (result == _requires_result::member)
			return promise.set_stop_token(::std::forward<decltype(token)>(token));
		else if constexpr (result == _requires_result::tag_invoke)
			return tag_invoke(set_stop_token_t{}, promise, ::std::forward<decltype(token)>(token));
	}
} set_stop_token{};

template<class Source>
struct forward_stop_request {
	Source& stop_source;

	constexpr decltype(auto) operator()() const noexcept
	{
		return stop_source.request_stop();
	}
};

template<class Source, class Callback>
struct connect_stop_token
{
	Source stop_source{};
	Callback stop_callback;
};


template<class Promise, class Parent>
concept direct_cpatrue_stop_token = requires(Promise& promise, Parent& parent)
{
	set_stop_token(promise, ::stdexec::get_stop_token(::stdexec::get_env(parent)));
};

template<class Parent, class Callback>
concept indirect_stop_token_provider =
	::stdexec::stoppable_token<::stdexec::stop_token_of_t<::stdexec::env_of_t<Parent>>>
	&& requires{ typename ::stdexec::stop_callback_for_t<::stdexec::stop_token_of_t<::stdexec::env_of_t<Parent>>, Callback>; }
;

namespace awaitable_traits
{
#if NAGISA_CONCURRENCY_USE_EXECUTION

	template<class Source, class Promise, class ParentPromise>
	struct capture_stop_token_t {};

	template<class Source, class Promise>
	struct capture_stop_token_t<Source, Promise, void>
	{
		using promise_type = Promise;
		using handle_type = ::std::coroutine_handle<promise_type>;
		using context_type = connect_stop_token<Source, ::std::any>;

		constexpr static auto create_context(handle_type self) noexcept
		{
			return context_type{};
		}

		template<class ParentPromise>
		constexpr static decltype(auto) await_suspend(context_type& context, handle_type self, ::std::coroutine_handle<ParentPromise> parent) noexcept
		{
			if constexpr (direct_cpatrue_stop_token<Promise, ParentPromise>)
			{
				set_stop_token(self.promise(), ::stdexec::get_stop_token(::stdexec::get_env(parent.promise())));
			}
			else if constexpr(indirect_stop_token_provider<ParentPromise, forward_stop_request<Source>>)
			{
				// using stop_token_type = ::stdexec::stop_token_of_t<::stdexec::env_of_t<ParentPromise>>;
				// using callback_type = ::stdexec::stop_callback_for_t<stop_token_type, forward_stop_request<Source>>;
				// context.stop_callback.template emplace<callback_type>(::stdexec::get_stop_token(::stdexec::get_env(parent.promise())), forward_stop_request<Source>(context.stop_source));
				// set_stop_token(self.promise(), context.stop_source.get_token());
			}
		}
	};
	template<class Source, class Promise, class ParentPromise>
		requires direct_cpatrue_stop_token<Promise, ParentPromise>
	struct capture_stop_token_t<Source, Promise, ParentPromise>
	{
		using promise_type = Promise;
		using handle_type = ::std::coroutine_handle<promise_type>;

		constexpr static decltype(auto) await_suspend(handle_type self, ::std::coroutine_handle<ParentPromise> parent) noexcept
		{
			set_stop_token(self.promise(), ::stdexec::get_stop_token(::stdexec::get_env(parent.promise())));
		}
	};
	template<class Source, class Promise, class ParentPromise>
		requires (!direct_cpatrue_stop_token<Promise, ParentPromise>) && indirect_stop_token_provider<ParentPromise, forward_stop_request<Source>>
		&& requires(Promise promise, Source source)
		{
			set_stop_token(promise, source.get_token());
		}
	struct capture_stop_token_t<Source, Promise, ParentPromise>
	{
		using promise_type = Promise;
		using handle_type = ::std::coroutine_handle<promise_type>;
		using stop_token_type = ::stdexec::stop_token_of_t<::stdexec::env_of_t<ParentPromise>>;
		using callback_type = ::stdexec::stop_callback_for_t<stop_token_type, forward_stop_request<Source>>;
		using context_type = connect_stop_token<Source, ::std::optional<callback_type>>;

		constexpr static auto create_context(handle_type self, ParentPromise& parent) noexcept
		{
			return context_type{};
		}

		constexpr static decltype(auto) await_suspend(context_type& context, handle_type self, ::std::coroutine_handle<ParentPromise> parent) noexcept
		{
			context.stop_callback.emplace(::stdexec::get_stop_token(::stdexec::get_env(parent.promise())), forward_stop_request<Source>(context.stop_source));
			set_stop_token(self.promise(), context.stop_source.get_token());
		}
	};

	template<class Source>
	struct capture_stop_token
	{
		template<class Promise, class Parent>
		using type = capture_stop_token_t<Source, Promise, Parent>;
	};

	template<class Promise, class Parent>
	using capture_inplace_stop_token = capture_stop_token_t<::stdexec::inplace_stop_source, Promise, Parent>;
#endif
}

namespace promises
{
	struct without_stop_token
	{
		[[nodiscard]] static ::std::coroutine_handle<> unhandled_stopped() noexcept
		{
			::std::terminate();
		}
	};

#if NAGISA_CONCURRENCY_USE_EXECUTION
	template<::stdexec::stoppable_token StopToken = ::stdexec::inplace_stop_token>
	struct with_stop_token : without_stop_token
	{
		using self_type = with_stop_token;
		using stop_token_type = StopToken;

		struct env_type
		{
			constexpr auto&& query(::stdexec::get_stop_token_t) const noexcept { return _self->_stop_token; }
			self_type const* _self;
		};
		constexpr auto get_env() const noexcept { return env_type{ this }; }

		[[nodiscard]] constexpr auto stop_requested() const noexcept
		{
			return _stop_token.stop_requested();
		}
		constexpr auto set_stop_token(auto&& token) noexcept
			requires ::std::assignable_from<stop_token_type&, decltype(token)>
		{
			_stop_token = ::std::forward<decltype(token)>(token);
		}
		stop_token_type _stop_token{};
	};

#endif
}

NAGISA_BUILD_LIB_DETAIL_END