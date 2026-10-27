#pragma once

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

/// @brief CPO：往 Store 里一次还 count 个（仅用于不带值的 Store，即计数器）。
///
/// Store 必须提供 @c give()（还一个）。@c give(count) 是可选的优化：
///   1. 提供了 @c store.give(count) —— 直接调用，通常是一次加法，O(1)。
///   2. 没提供 —— 退化成循环调用 @c give() 手拼 count 次。
///
/// 调用方（acquire_core::_release_n）是在持锁状态下调用它的，所以能提供
/// O(1) 的 give(count) 的 Store 应该提供，否则持锁时间会与 count 成正比。
inline constexpr struct give_n_t
{
	template <class Store, class Count>
	static consteval bool _native() noexcept
	{
		return requires(Store& store, Count count) { store.give(count); };
	}
	template <class Store, class Count>
	static consteval bool _noexcept() noexcept
	{
		// 惰性求值：没有 give(count) 时不能去看它的 noexcept。
		if constexpr (_native<Store, Count>())
			return noexcept(::std::declval<Store&>().give(::std::declval<Count>()));
		else
			return noexcept(::std::declval<Store&>().give());
	}

	template <class Store, ::std::integral Count>
		requires requires(Store& store) { store.give(); }
	constexpr void operator()(Store& store, Count count) const noexcept(_noexcept<Store, Count>())
	{
		if constexpr (_native<Store, Count>())
			store.give(count);
		else
			for (; count != 0; --count)
				store.give();
	}
} give_n{};

NAGISA_BUILD_LIB_DETAIL_END
