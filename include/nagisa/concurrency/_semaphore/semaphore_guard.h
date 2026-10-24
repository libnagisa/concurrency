#pragma once

#include "./basic_semaphore.h"

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

// 对 basic_semaphore 的薄 RAII 适配：内部持有一个 basic_semaphore，自己不
// 重新实现等待队列/取消那套状态机，也不重新发明 sender/operation —— acquire()
// 直接复用 basic_semaphore::acquire() 返回的 sender，用 stdexec::then 在完成
// 时把无值信号转换成一个 permit；permit 析构时调用 release(1) 自动归还。
//
// 因此不支持 release(n) 批量释放，也不支持“没有对应 acquire 就直接 release”
// 的事件通知式用法——那些用法没有一个自然的持有者对象可以挂 RAII，需要这类
// 用法的调用方应该直接用 basic_semaphore（或它的别名 counting_semaphore /
// binary_semaphore），不要用 semaphore_guard。
template <::std::ptrdiff_t Max = (::std::numeric_limits<::std::ptrdiff_t>::max)()>
struct basic_semaphore_guard
{
private:
	using self_type = basic_semaphore_guard;
	using inner_type = basic_semaphore<Max>;
public:
	static constexpr ::std::ptrdiff_t max() noexcept { return inner_type::max(); }

	struct permit
	{
		inner_type* _semaphore = nullptr;

		constexpr permit() = default;
		constexpr explicit permit(inner_type& semaphore) noexcept
			: _semaphore(::std::addressof(semaphore))
		{
		}
		constexpr permit(permit&& other) noexcept
			: _semaphore(::std::exchange(other._semaphore, nullptr))
		{
		}
		constexpr permit& operator=(permit&& other) noexcept
		{
			auto discarded = ::std::move(*this);
			_semaphore = ::std::exchange(other._semaphore, nullptr);
			return *this;
		}
		constexpr permit(permit const&) noexcept = delete;
		constexpr permit& operator=(permit const&) noexcept = delete;
		constexpr ~permit()
		{
			if (_semaphore != nullptr)
				_semaphore->release();
		}

		[[nodiscard]] constexpr explicit operator bool() const noexcept { return _semaphore != nullptr; }
	};

	inner_type _semaphore;

	constexpr explicit basic_semaphore_guard(::std::ptrdiff_t desired) noexcept
		: _semaphore(desired)
	{
	}

	constexpr basic_semaphore_guard(self_type&&) noexcept = delete;
	constexpr basic_semaphore_guard(self_type const&) noexcept = delete;
	constexpr self_type& operator=(self_type&&) noexcept = delete;
	constexpr self_type& operator=(self_type const&) noexcept = delete;

	[[nodiscard]] constexpr auto acquire() noexcept
	{
		return _semaphore.acquire()
			| ::stdexec::then([this]() noexcept { return permit{ _semaphore }; });
	}
	[[nodiscard]] ::std::optional<permit> try_acquire() noexcept
	{
		if (_semaphore.try_acquire())
			return ::std::optional<permit>{ ::std::in_place, _semaphore };
		return ::std::nullopt;
	}
};

template <::std::ptrdiff_t Max = (::std::numeric_limits<::std::ptrdiff_t>::max)()>
using counting_semaphore_guard = basic_semaphore_guard<Max>;

using binary_semaphore_guard = basic_semaphore_guard<1>;

NAGISA_BUILD_LIB_DETAIL_END
