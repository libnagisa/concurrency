#pragma once

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

enum class result_status : ::std::uint8_t
{
	result,
	exception,
};

template<class Result, bool Throw>
struct promise_base
{
	constexpr auto result_state() const noexcept { return static_cast<result_status>(_data.index()); }
	constexpr decltype(auto) get_result() noexcept { return ::std::get<::std::to_underlying(result_status::result)>(_data); }
	constexpr decltype(auto) get_exception() noexcept { return ::std::get<::std::to_underlying(result_status::exception)>(_data); }

	::std::variant<::std::optional<Result>, ::std::exception_ptr> _data{ ::std::nullopt };
};
template<class Result>
struct promise_base<Result, false>
{
	constexpr static auto result_state() noexcept { return result_status::result; }
	constexpr decltype(auto) get_result() noexcept { return _data; }
	static ::std::exception_ptr get_exception() noexcept { ::std::abort(); }

	::std::optional<Result> _data = ::std::nullopt;
};
template<>
struct promise_base<void, true>
{
	constexpr static auto result_state() noexcept { return result_status::result; }
	constexpr static auto get_result() noexcept {}
	static ::std::exception_ptr get_exception() noexcept { ::std::abort(); }

	::std::exception_ptr _data = nullptr;
};
template<>
struct promise_base<void, false>
{
	constexpr static auto result_state() noexcept { return result_status::result; }
	constexpr static auto get_result() noexcept { }
	static ::std::exception_ptr get_exception() noexcept { ::std::abort(); }
};



template<class R = void>
struct task
{
	using self_type = task;

	struct promise_type;
	using handle_type = std::coroutine_handle<promise_type>;
	struct promise_type
	{
		constexpr auto get_return_object() noexcept { return task{ handle_type::from_promise(*this) }; }
		constexpr static auto initial_suspend() noexcept { return ::std::suspend_always{}; }
		constexpr static auto final_suspend() noexcept
		{
			struct final_awaiter
			{
				constexpr static auto await_ready() noexcept { return false; }
				constexpr auto await_suspend(::std::coroutine_handle<promise_type> h) const noexcept
				{
					return h.promise()._continuation;
				}
				constexpr static auto await_resume() noexcept { ::std::abort(); }
			};
			return final_awaiter{};
		}

		handle_type _continuation{};
	};

	constexpr explicit task(handle_type h) noexcept : _handle(h) {}
	constexpr task(self_type const&) noexcept = delete;
	constexpr task(self_type&& t) noexcept : _handle(::std::exchange(t._handle, {})) {}
	constexpr self_type& operator=(self_type const&) noexcept = delete;
	constexpr self_type& operator=(self_type&&) noexcept = delete;

	constexpr ~task() noexcept
	{
		if (_handle)
			_handle.destroy();
	}


	constexpr auto operator co_await() && noexcept
	{
		struct awaiter
		{
			~awaiter() noexcept
			{
				if (handle) [[likely]]
					handle.destroy();
			}
			constexpr static auto await_ready() noexcept { return false; }
			auto await_suspend(::std::coroutine_handle<> continuation) const noexcept
			{
				handle.promise()._continuation = continuation;
				return handle;
			}
			auto await_resume() const
			{
				if (handle.promise()._exception) [[unlikely]]
					::std::rethrow_exception(handle.promise()._exception);
			}
			handle_type handle;
		};
		return awaiter{ ::std::exchange(_handle, {}) };
	}

	handle_type _handle{};
};

NAGISA_BUILD_LIB_DETAIL_END