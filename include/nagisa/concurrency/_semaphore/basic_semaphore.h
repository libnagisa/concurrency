#pragma once

#include "./acquire_core.h"
#include "./environment.h"

#include "./macro_push.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

// 计数器可以用的整数类型：标准整数类型，不含 bool 和字符类型
// （下面的范围检查靠 std::cmp_*，它们不接受这些类型）。
template <class T>
concept counter_integral =
	::std::integral<T> &&
	!::std::same_as<::std::remove_cv_t<T>, bool> &&
	!::std::same_as<::std::remove_cv_t<T>, char> &&
	!::std::same_as<::std::remove_cv_t<T>, wchar_t> &&
	!::std::same_as<::std::remove_cv_t<T>, char8_t> &&
	!::std::same_as<::std::remove_cv_t<T>, char16_t> &&
	!::std::same_as<::std::remove_cv_t<T>, char32_t>;

// acquire_core 的 Store：一个计数器，不带值。
// Counter 是计数器的存储类型，上限是 numeric_limits<Counter>::max()。
// 越界（超过上限、负数）都是前置条件违反，用断言检查，release 构建下零开销。
template <counter_integral Counter>
struct counter_store
{
	using value_type = void;

	static constexpr Counter max() noexcept { return (::std::numeric_limits<Counter>::max)(); }

	Counter _count;

	constexpr explicit counter_store(::std::ptrdiff_t desired) noexcept
		: _count(static_cast<Counter>(desired))
	{
		NAGISA_CONCURRENCY_SEMAPHORE_ASSERT(desired >= 0 && ::std::cmp_less_equal(desired, max()));
	}

	[[nodiscard]] constexpr bool available() const noexcept { return _count > 0; }
	constexpr void take() noexcept
	{
		NAGISA_CONCURRENCY_SEMAPHORE_ASSERT(_count > 0);
		--_count;
	}
	// 必须提供：还一个。
	constexpr void give() noexcept
	{
		NAGISA_CONCURRENCY_SEMAPHORE_ASSERT(_count < max());
		++_count;
	}
	// 可选：一次还 count 个（O(1)），由 give_n 优先使用。
	constexpr void give(::std::ptrdiff_t count) noexcept
	{
		NAGISA_CONCURRENCY_SEMAPHORE_ASSERT(count >= 0 && ::std::cmp_less_equal(count, max() - _count));
		_count = static_cast<Counter>(_count + count);
	}
};

// LeastMaxValue 对齐 std::counting_semaphore：保证至少能容纳这么多个许可，
// 不是精确上限。实际上限是 max()，由 Counter 决定。
template <::std::ptrdiff_t LeastMaxValue = (::std::numeric_limits<::std::ptrdiff_t>::max)(),
	counter_integral Counter = ::std::ptrdiff_t>
struct basic_semaphore
{
private:
	using self_type = basic_semaphore;
public:
	static_assert(LeastMaxValue >= 0);
	static_assert(::std::cmp_less_equal(LeastMaxValue, (::std::numeric_limits<Counter>::max)()),
		"Counter is too narrow to hold LeastMaxValue permits");

	using counter_type = Counter;
	using core_type = acquire_core<counter_store<Counter>>;
	static constexpr Counter max() noexcept { return counter_store<Counter>::max(); }

	using acquire_sender = typename core_type::acquire_sender;

	core_type _core;

	constexpr explicit basic_semaphore(::std::ptrdiff_t desired) noexcept
		: _core(::std::in_place, desired)
	{
	}

	constexpr basic_semaphore(self_type&&) noexcept = delete;
	constexpr basic_semaphore(self_type const&) noexcept = delete;
	constexpr self_type& operator=(self_type&&) noexcept = delete;
	constexpr self_type& operator=(self_type const&) noexcept = delete;

	[[nodiscard]] constexpr acquire_sender acquire() noexcept { return _core.acquire(); }
	[[nodiscard]] bool try_acquire() noexcept { return _core._try_take(); }
	void release(::std::ptrdiff_t update = 1) noexcept { _core._release_n(update); }
};

NAGISA_BUILD_LIB_DETAIL_END

#include "./macro_pop.h"
