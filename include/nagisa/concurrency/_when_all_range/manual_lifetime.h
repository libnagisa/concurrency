#pragma once

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

template <class ValueType>
struct manual_lifetime
{
	constexpr manual_lifetime() noexcept = default;
	constexpr manual_lifetime(manual_lifetime&&) = delete;
	constexpr manual_lifetime(manual_lifetime const&) = delete;
	constexpr auto operator=(manual_lifetime&&) -> manual_lifetime & = delete;
	constexpr auto operator=(manual_lifetime const&) -> manual_lifetime & = delete;
	constexpr ~manual_lifetime() noexcept = default;

	constexpr ValueType& construct(auto&&... arguments)
		noexcept(::std::is_nothrow_constructible_v<ValueType, decltype(arguments)...>)
	{
		// 用 placement new 而不是 `::std::construct_at`：前者支持聚合初始化的花括号省略。
		return *::std::launder(
			::new (static_cast<void*>(_buffer)) ValueType{ ::std::forward<decltype(arguments)>(arguments)... });
	}
	constexpr auto construct_from(auto&& function, auto&&... arguments) -> ValueType&
	{
		// 同样用 placement new：返回值可能是不可移动的类型（op-state 正是），
		// `::std::construct_at` 那条路会去找移动构造。
		return *::std::launder(::new (static_cast<void*>(_buffer))
			ValueType{ ::std::forward<decltype(function)>(function)(::std::forward<decltype(arguments)>(arguments)...) });
	}
	constexpr auto destroy() noexcept -> void { ::std::destroy_at(&get()); }
	[[nodiscard]] constexpr auto get() & noexcept -> ValueType&
	{
		return *reinterpret_cast<ValueType*>(_buffer);
	}
	[[nodiscard]] constexpr auto get() && noexcept -> ValueType&&
	{
		return static_cast<ValueType&&>(*reinterpret_cast<ValueType*>(_buffer));
	}
	[[nodiscard]] constexpr auto get() const& noexcept -> ValueType const&
	{
		return *reinterpret_cast<ValueType const*>(_buffer);
	}
	constexpr auto get() const&& noexcept -> ValueType const&& = delete;
	[[nodiscard]] constexpr auto operator->() noexcept -> ValueType*
	{
		return reinterpret_cast<ValueType*>(_buffer);
	}
	[[nodiscard]] constexpr auto operator->() const noexcept -> ValueType const*
	{
		return reinterpret_cast<ValueType const*>(_buffer);
	}
	alignas(ValueType) unsigned char _buffer[sizeof(ValueType)]{};
};

template <class ReferenceType>
	requires ::std::is_reference_v<ReferenceType>
struct manual_lifetime<ReferenceType>
{
	constexpr manual_lifetime() noexcept = default;
	constexpr manual_lifetime(manual_lifetime&&) = delete;
	constexpr manual_lifetime(manual_lifetime const&) = delete;
	constexpr auto operator=(manual_lifetime&&) -> manual_lifetime & = delete;
	constexpr auto operator=(manual_lifetime const&) -> manual_lifetime & = delete;
	constexpr ~manual_lifetime() noexcept = default;

	constexpr auto construct(ReferenceType reference) noexcept -> ReferenceType
	{
		_pointer = ::std::addressof(reference);
		return static_cast<ReferenceType>(*_pointer);
	}

	constexpr auto construct_from(auto&& function, auto&&... arguments)
		noexcept(::std::is_nothrow_invocable_v<decltype(function), decltype(arguments)...>) -> ReferenceType
	{
		decltype(auto) result = ::std::forward<decltype(function)>(function)(::std::forward<decltype(arguments)>(arguments)...);
		static_assert(::std::is_reference_v<decltype(result)>, "the result must be a reference");
		_pointer = ::std::addressof(result);
		return static_cast<ReferenceType>(*_pointer);
	}
	constexpr static void destroy() noexcept {}
	[[nodiscard]] constexpr auto get() const noexcept -> ReferenceType
	{
		return static_cast<ReferenceType>(*_pointer);
	}
	[[nodiscard]] constexpr auto operator->() const noexcept -> ::std::add_pointer_t<ReferenceType>
	{
		return _pointer;
	}
	::std::add_pointer_t<ReferenceType> _pointer = nullptr;
};

/// `void` 特化：什么都不存，调用 `construct_from` 时只是把函数跑一遍。
template <>
struct manual_lifetime<void>
{
	constexpr manual_lifetime() noexcept = default;
	constexpr manual_lifetime(manual_lifetime&&) = delete;
	constexpr manual_lifetime(manual_lifetime const&) = delete;
	constexpr auto operator=(manual_lifetime&&) -> manual_lifetime & = delete;
	constexpr auto operator=(manual_lifetime const&) -> manual_lifetime & = delete;
	constexpr ~manual_lifetime() noexcept = default;

	constexpr static void construct(auto&&...) noexcept {}
	constexpr auto construct_from(auto&& function, auto&&... arguments) noexcept -> void
	{
		static_cast<void>(::std::forward<decltype(function)>(function)(::std::forward<decltype(arguments)>(arguments)...));
	}
	constexpr static void destroy() noexcept {}
	constexpr static void get() noexcept {}
	[[nodiscard]] constexpr void* operator->() const noexcept { return nullptr; }
};


NAGISA_BUILD_LIB_DETAIL_END