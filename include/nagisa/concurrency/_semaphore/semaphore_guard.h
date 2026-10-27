#pragma once

#include "./basic_semaphore.h"

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

// 对 basic_semaphore 的薄 RAII 适配：内部持有一个 basic_semaphore，自己不
// 重新实现等待队列/取消那套状态机，也不重新发明 sender/operation —— acquire()
// 返回同一个 acquire_core 的 permit 版 sender（完成时直接交出 permit，不再用
// then 之类做二次包装）；permit 析构时 release(1) 自动归还。
//
// 因此不支持 release(n) 批量释放，也不支持“没有对应 acquire 就直接 release”
// 的事件通知式用法——那些用法没有一个自然的持有者对象可以挂 RAII，需要这类
// 用法的调用方应该直接用 basic_semaphore（或它的别名 counting_semaphore /
// binary_semaphore），不要用 semaphore_guard。
template <::std::ptrdiff_t LeastMaxValue = (::std::numeric_limits<::std::ptrdiff_t>::max)()>
struct basic_semaphore_guard
{
private:
	using self_type = basic_semaphore_guard;
	using inner_type = basic_semaphore<LeastMaxValue>;
public:
	static constexpr auto max() noexcept { return inner_type::max(); }

	using permit = typename inner_type::core_type::permit_type;

	inner_type _semaphore;

	constexpr explicit basic_semaphore_guard(::std::ptrdiff_t desired) noexcept
		: _semaphore(desired)
	{
	}

	constexpr basic_semaphore_guard(self_type&&) noexcept = delete;
	constexpr basic_semaphore_guard(self_type const&) noexcept = delete;
	constexpr self_type& operator=(self_type&&) noexcept = delete;
	constexpr self_type& operator=(self_type const&) noexcept = delete;

	[[nodiscard]] constexpr auto acquire() noexcept { return _semaphore._core.acquire_permit(); }
	[[nodiscard]] ::std::optional<permit> try_acquire() noexcept
	{
		if (_semaphore.try_acquire())
			return ::std::optional<permit>{ ::std::in_place, _semaphore._core };
		return ::std::nullopt;
	}
};

template <::std::ptrdiff_t LeastMaxValue = (::std::numeric_limits<::std::ptrdiff_t>::max)()>
using counting_semaphore_guard = basic_semaphore_guard<LeastMaxValue>;

using binary_semaphore_guard = basic_semaphore_guard<1>;

NAGISA_BUILD_LIB_DETAIL_END
