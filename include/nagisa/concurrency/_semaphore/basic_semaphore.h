#pragma once

#include "./acquire_core.h"
#include "./environment.h"

#include "./macro_push.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

// acquire_core 的 Store：一个计数器，不带值。
struct counter_store
{
	using value_type = void;

	::std::ptrdiff_t _count;

	constexpr explicit counter_store(::std::ptrdiff_t desired) noexcept
		: _count(desired)
	{
	}

	[[nodiscard]] constexpr bool available() const noexcept { return _count > 0; }
	constexpr void take() noexcept { --_count; }
	constexpr void give() noexcept { ++_count; }
};

template <::std::ptrdiff_t Max = (::std::numeric_limits<::std::ptrdiff_t>::max)()>
struct basic_semaphore
{
private:
	using self_type = basic_semaphore;
	using core_type = acquire_core<counter_store>;
public:
	static constexpr ::std::ptrdiff_t max() noexcept { return Max; }

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
		for (; update != 0; --update)
			_core._release();
	}
};

NAGISA_BUILD_LIB_DETAIL_END

#include "./macro_pop.h"
