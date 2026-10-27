#pragma once

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

// acquire_core 在 Store 带有值（value_type 非 void）时交给接收者的 RAII 凭据：
// 持有取出的值，析构时把它还回 core（若有等待者则直接交给队首）。
template <class Core>
struct resource_permit
{
private:
	using self_type = resource_permit;
public:
	using core_type = Core;
	using token_type = typename Core::value_type;

	core_type* _core = nullptr;
	token_type _token{};

	constexpr resource_permit() = default;
	constexpr resource_permit(core_type& core, token_type token) noexcept
		: _core(::std::addressof(core))
		, _token(::std::move(token))
	{
	}
	constexpr resource_permit(self_type&& other) noexcept
		: _core(::std::exchange(other._core, nullptr))
		, _token(::std::exchange(other._token, token_type{}))
	{
	}
	constexpr self_type& operator=(self_type&& other) noexcept
	{
		auto discarded = ::std::move(*this);
		_core = ::std::exchange(other._core, nullptr);
		_token = ::std::exchange(other._token, token_type{});
		return *this;
	}
	constexpr resource_permit(self_type const&) noexcept = delete;
	constexpr self_type& operator=(self_type const&) noexcept = delete;
	constexpr ~resource_permit()
	{
		if (_core != nullptr)
			_core->_release(::std::move(_token));
	}

	[[nodiscard]] constexpr explicit operator bool() const noexcept { return _core != nullptr; }
	[[nodiscard]] constexpr auto&& token() noexcept { return _token; }
	[[nodiscard]] constexpr auto&& token() const noexcept { return _token; }
};

// 不带值的 Store（计数器）的 RAII 凭据：析构时 _release() 一个许可。
template <class Core>
struct counter_permit
{
private:
	using self_type = counter_permit;
public:
	using core_type = Core;

	core_type* _core = nullptr;

	constexpr counter_permit() = default;
	constexpr explicit counter_permit(core_type& core) noexcept
		: _core(::std::addressof(core))
	{
	}
	constexpr counter_permit(self_type&& other) noexcept
		: _core(::std::exchange(other._core, nullptr))
	{
	}
	constexpr self_type& operator=(self_type&& other) noexcept
	{
		auto discarded = ::std::move(*this);
		_core = ::std::exchange(other._core, nullptr);
		return *this;
	}
	constexpr counter_permit(self_type const&) noexcept = delete;
	constexpr self_type& operator=(self_type const&) noexcept = delete;
	constexpr ~counter_permit()
	{
		if (_core != nullptr)
			_core->_release();
	}

	[[nodiscard]] constexpr explicit operator bool() const noexcept { return _core != nullptr; }
};

NAGISA_BUILD_LIB_DETAIL_END
