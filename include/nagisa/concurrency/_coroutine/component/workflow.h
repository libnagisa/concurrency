#pragma once

#include "../promise.h"
#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

inline constexpr struct set_continuation_t
{
	enum class _requires_result
	{
		member,
		tag_invoke,
		none,
	};
	template<class Promise, class ParentPromise>
	consteval static auto _check() noexcept
	{
		if constexpr (requires(Promise & promise, ::std::coroutine_handle<ParentPromise> parent) { promise.set_continuation(parent); })
			return _requires_result::member;
		else if constexpr (requires(Promise & promise, ::std::coroutine_handle<ParentPromise> parent) { tag_invoke(set_continuation_t{}, promise, parent); })
			return _requires_result::tag_invoke;
		return _requires_result::none;
	}
	template<class ParentPromise>
	constexpr decltype(auto) operator()(auto& promise, ::std::coroutine_handle<ParentPromise> parent) const noexcept
		requires (_check<decltype(promise), ParentPromise>() != _requires_result::none)
	{
		constexpr auto result = _check<decltype(promise), ParentPromise>();
		if constexpr (result == _requires_result::member)
			return promise.set_continuation(parent);
		else if constexpr (result == _requires_result::tag_invoke)
			return tag_invoke(set_continuation_t{}, promise, parent);
	}
} set_continuation{};

inline constexpr struct continuation_t
{
	enum class _requires_result
	{
		member,
		tag_invoke,
		none,
	};
	template<class Promise>
	consteval static auto _check() noexcept
	{
		if constexpr (requires(Promise & promise) { promise.continuation(); })
			return _requires_result::member;
		else if constexpr (requires(Promise & promise) { tag_invoke(continuation_t{}, promise); })
			return _requires_result::tag_invoke;
		return _requires_result::none;
	}
	constexpr decltype(auto) operator()(auto& promise) const noexcept
		requires (_check<decltype(promise)>() != _requires_result::none)
	{
		constexpr auto result = _check<decltype(promise)>();
		if constexpr (result == _requires_result::member)
			return promise.continuation();
		else if constexpr (result == _requires_result::tag_invoke)
			return tag_invoke(continuation_t{}, promise);
	}
} continuation{};

namespace awaitable_traits
{
	template<class Promise, class ParentPromise>
	struct trivial_suspend
	{
		using promise_type = Promise;
		using handle_type = ::std::coroutine_handle<promise_type>;

		constexpr static decltype(auto) await_suspend(handle_type self, ::std::coroutine_handle<ParentPromise> parent) noexcept
		{
		}
	};

	template<class Promise, class ParentPromise>
	struct run_this
	{
		using promise_type = Promise;
		using handle_type = ::std::coroutine_handle<promise_type>;

		constexpr static decltype(auto) await_suspend(handle_type self, ::std::coroutine_handle<ParentPromise> parent) noexcept
		{
			return self;
		}
	};

	template<class Promise, class ParentPromise>
	struct run_parent
	{
		using promise_type = Promise;
		using handle_type = ::std::coroutine_handle<promise_type>;

		constexpr static decltype(auto) await_suspend(handle_type self, ::std::coroutine_handle<ParentPromise> parent) noexcept
		{
			return parent;
		}
	};

	template<class Promise, class ParentPromise>
		requires ::std::invocable<set_continuation_t, Promise&, ::std::coroutine_handle<ParentPromise>>
	struct this_then_parent
	{
		using promise_type = Promise;
		using handle_type = ::std::coroutine_handle<promise_type>;

		constexpr static decltype(auto) await_suspend(handle_type self, ::std::coroutine_handle<ParentPromise> parent) noexcept
		{
			set_continuation(self.promise(), parent);
		}
	};

	template<class Promise, class ParentPromise>
		requires
			::std::invocable<set_continuation_t, Promise&, ::std::coroutine_handle<ParentPromise>>
			&& ::std::invocable<set_continuation_t, ParentPromise&, ::std::coroutine_handle<Promise>>
			&& requires(ParentPromise& parent)
			{
				continuation(parent);
				requires ::std::constructible_from<::std::coroutine_handle<>, decltype(continuation(parent))>;
			}
	struct parent_then_this
	{
		using promise_type = Promise;
		using handle_type = ::std::coroutine_handle<promise_type>;

		constexpr static decltype(auto) await_suspend(handle_type self, ::std::coroutine_handle<ParentPromise> parent) noexcept
		{
			set_continuation(self.promise(), continuation(parent));
			set_continuation(parent, self);
		}
	};

	template<class Promise, class ParentPromise>
	struct destroy_after_resumed
	{
		using promise_type = Promise;
		using handle_type = ::std::coroutine_handle<promise_type>;
		constexpr static decltype(auto) await_resume(handle_type self) noexcept
		{
			self.destroy();
		}
	};
}

namespace promises
{
	struct jump_to_continuation
	{
		constexpr jump_to_continuation() = default;
		constexpr auto final_suspend() const noexcept
		{
			struct awaitable
			{
				constexpr static auto await_ready() noexcept { return false; }
				constexpr auto await_suspend(::std::coroutine_handle<>) const noexcept { return continuation; }
				[[noreturn]] static auto await_resume() noexcept { ::std::abort(); }

				::std::coroutine_handle<> continuation;
			};
			return awaitable{_continuation};
		}

		constexpr auto set_continuation(::std::coroutine_handle<> continuation) noexcept
		{
			_continuation = continuation;
		}

		constexpr auto continuation() const noexcept { return _continuation; }

		::std::coroutine_handle<> _continuation = ::std::noop_coroutine();
	};

	struct trivial_exit
	{
		constexpr static auto final_suspend() noexcept { return ::std::suspend_always{}; }
	};
}

NAGISA_BUILD_LIB_DETAIL_END