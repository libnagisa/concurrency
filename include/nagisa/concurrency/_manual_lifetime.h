#pragma once

#include <type_traits>
#include <new>
#include <memory>
#include <utility>

#include <nagisa/concurrency/environment.h>

NAGISA_BUILD_LIB_DETAIL_BEGIN

template <class ValueType>
struct manual_lifetime
{
	using self_type = manual_lifetime;
	using value_type = ValueType;

	constexpr manual_lifetime() noexcept = default;
	constexpr manual_lifetime(self_type&&) = delete;
	constexpr manual_lifetime(self_type const&) = delete;
	constexpr self_type& operator=(self_type&&) = delete;
	constexpr self_type& operator=(self_type const&) = delete;
	constexpr ~manual_lifetime() noexcept = default;

	constexpr value_type& construct(auto&&... arguments)
		noexcept(::std::is_nothrow_constructible_v<value_type, decltype(arguments)...>)
	{
		// 用 placement new 而不是 `::std::construct_at`：前者支持聚合初始化的花括号省略。
		return *::std::launder(::new (static_cast<void*>(_buffer)) value_type{ ::std::forward<decltype(arguments)>(arguments)... });
	}
	constexpr value_type& construct_from(auto&& function, auto&&... arguments)
		noexcept(::std::is_nothrow_invocable_v<decltype(function), decltype(arguments)...>)
	{
		// 同样用 placement new：返回值可能是不可移动的类型（op-state 正是），
		// `::std::construct_at` 那条路会去找移动构造。
		return *::std::launder(::new (static_cast<void*>(_buffer)) value_type{ ::std::invoke(::std::forward<decltype(function)>(function), ::std::forward<decltype(arguments)>(arguments)...) });
	}
	constexpr void destroy() noexcept { ::std::destroy_at(&get()); }
	[[nodiscard]] constexpr value_type& get() & noexcept { return *reinterpret_cast<value_type*>(_buffer); }
	[[nodiscard]] constexpr value_type&& get() && noexcept { return static_cast<value_type&&>(*reinterpret_cast<value_type*>(_buffer)); }
	[[nodiscard]] constexpr value_type const& get() const& noexcept { return *reinterpret_cast<value_type const*>(_buffer); }
	constexpr value_type const&& get() const&& noexcept = delete;
	[[nodiscard]] constexpr value_type* operator->() noexcept { return reinterpret_cast<value_type*>(_buffer); }
	[[nodiscard]] constexpr value_type const* operator->() const noexcept { return reinterpret_cast<value_type const*>(_buffer); }
	alignas(value_type) unsigned char _buffer[sizeof(value_type)]{};
};

template <class ReferenceType>
	requires ::std::is_reference_v<ReferenceType>
struct manual_lifetime<ReferenceType>
{
	using self_type = manual_lifetime;
	using reference_type = ReferenceType;

	constexpr manual_lifetime() noexcept = default;
	constexpr manual_lifetime(self_type&&) = delete;
	constexpr manual_lifetime(self_type const&) = delete;
	constexpr self_type& operator=(self_type&&) = delete;
	constexpr self_type& operator=(self_type const&) = delete;
	constexpr ~manual_lifetime() noexcept = default;

	constexpr reference_type construct(reference_type reference) noexcept
	{
		_pointer = ::std::addressof(reference);
		return static_cast<reference_type>(*_pointer);
	}

	constexpr reference_type construct_from(auto&& function, auto&&... arguments)
		noexcept(::std::is_nothrow_invocable_v<decltype(function), decltype(arguments)...>)
	{
		decltype(auto) result = ::std::invoke(::std::forward<decltype(function)>(function), ::std::forward<decltype(arguments)>(arguments)...);
		static_assert(::std::is_reference_v<decltype(result)>, "the result must be a reference");
		_pointer = ::std::addressof(result);
		return static_cast<reference_type>(*_pointer);
	}
	constexpr static void destroy() noexcept {}
	[[nodiscard]] constexpr reference_type get() const noexcept { return static_cast<reference_type>(*_pointer); }
	[[nodiscard]] constexpr ::std::add_pointer_t<reference_type> operator->() const noexcept { return _pointer; }
	::std::add_pointer_t<reference_type> _pointer = nullptr;
};

/// `void` 特化：什么都不存，调用 `construct_from` 时只是把函数跑一遍。
template <>
struct manual_lifetime<void>
{
	using self_type = manual_lifetime;

	constexpr manual_lifetime() noexcept = default;
	constexpr manual_lifetime(self_type&&) = delete;
	constexpr manual_lifetime(self_type const&) = delete;
	constexpr self_type& operator=(self_type&&) = delete;
	constexpr self_type& operator=(self_type const&) = delete;
	constexpr ~manual_lifetime() noexcept = default;

	constexpr static void construct(auto&&...) noexcept {}
	constexpr void construct_from(auto&& function, auto&&... arguments) 
		noexcept(::std::is_nothrow_invocable_v<decltype(function), decltype(arguments)...>)
	{
		static_cast<void>(::std::invoke(::std::forward<decltype(function)>(function), ::std::forward<decltype(arguments)>(arguments)...));
	}
	constexpr static void destroy() noexcept {}
	constexpr static void get() noexcept {}
	[[nodiscard]] constexpr void* operator->() const noexcept { return nullptr; }
};


NAGISA_BUILD_LIB_DETAIL_END


#include <nagisa/build_lib/destruct.h>