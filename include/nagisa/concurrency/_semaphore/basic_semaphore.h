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

// acquire_core 的 Store：一个标志位，只能持有 0 或 1 个许可。
// 手写的二元信号量就是一个 bool：没有计数、没有加法、没有上限比较。
// 不提供 give(count)，批量版由 give_n 退化成循环调用 give()；
// 二元信号量里一次最多只有 1 个许可有意义，重复 give 是前置条件违反。
struct flag_store
{
	using value_type = void;

	static constexpr ::std::ptrdiff_t max() noexcept { return 1; }

	bool _free;

	constexpr explicit flag_store(::std::ptrdiff_t desired) noexcept
		: _free(desired != 0)
	{
		NAGISA_CONCURRENCY_SEMAPHORE_ASSERT(desired == 0 || desired == 1);
	}

	[[nodiscard]] constexpr bool available() const noexcept { return _free; }
	constexpr void take() noexcept
	{
		NAGISA_CONCURRENCY_SEMAPHORE_ASSERT(_free);
		_free = false;
	}
	constexpr void give() noexcept
	{
		NAGISA_CONCURRENCY_SEMAPHORE_ASSERT(!_free);
		_free = true;
	}
};

// 默认的存储：LeastMaxValue 为 1 时就是手写二元信号量的样子（一个标志位），
// 否则是 ptrdiff_t 计数器。显式指定 Counter 则总是用该类型的计数器。
struct default_counter_t {};

template <::std::ptrdiff_t LeastMaxValue, class Counter>
consteval auto semaphore_store_for() noexcept
{
	if constexpr (!::std::same_as<Counter, default_counter_t>)
		return ::std::type_identity<counter_store<Counter>>{};
	else if constexpr (LeastMaxValue == 1)
		return ::std::type_identity<flag_store>{};
	else
		return ::std::type_identity<counter_store<::std::ptrdiff_t>>{};
}

// LeastMaxValue 对齐 std::counting_semaphore：保证至少能容纳这么多个许可，
// 不是精确上限。实际上限是 max()，由存储决定：LeastMaxValue 为 1 且没有显式
// 指定 Counter 时是一个标志位（上限恰好 1，重复 release 是前置条件违反）；
// 否则是 Counter（默认 ptrdiff_t）的上限。
template <::std::ptrdiff_t LeastMaxValue = (::std::numeric_limits<::std::ptrdiff_t>::max)(),
	class Counter = default_counter_t>
struct basic_semaphore
{
private:
	using self_type = basic_semaphore;
public:
	static_assert(LeastMaxValue >= 0);
	using store_type = typename decltype(semaphore_store_for<LeastMaxValue, Counter>())::type;
	static_assert(::std::cmp_less_equal(LeastMaxValue, store_type::max()),
		"the store is too narrow to hold LeastMaxValue permits");

	using core_type = acquire_core<store_type>;
	static constexpr auto max() noexcept { return store_type::max(); }

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
	void release(::std::ptrdiff_t update = 1) noexcept
	{
		// 最常见的 update == 1 直接走单个归还：批量路径要在栈上建本地链表，
		// 单个路径不需要，也就没有那份栈保护和多余指令。
		if (update == 1)
			_core._release();
		else
			_core._release_n(update);
	}
};

NAGISA_BUILD_LIB_DETAIL_END

#include "./macro_pop.h"
