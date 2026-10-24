#pragma once

#include "./token.h"
#include "./available_container.h"
#include "../acquire_core.h"

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

// acquire_core 的 Store：一个可用 token 的容器，取出的就是 token。
template <resource_token Token, available_container<Token> AvailableContainer>
struct container_store
{
	using value_type = Token;

	AvailableContainer _container{};

	constexpr container_store() noexcept requires ::std::default_initializable<AvailableContainer> = default;
	template <class... Args>
		requires ::std::constructible_from<AvailableContainer, Args...>
	constexpr explicit container_store(Args&&... args) noexcept
		: _container(::std::forward<Args>(args)...)
	{
	}

	[[nodiscard]] constexpr bool available() const { return !::std::ranges::empty(_container); }
	[[nodiscard]] constexpr Token take()
	{
		Token token = ::std::move(_container.back());
		if constexpr (noexcept(_container.pop_back()))
		{
			_container.pop_back();
			return token;
		}
		else
		{
			try
			{
				_container.pop_back();
			}
			catch (...)
			{
				_container.back() = ::std::move(token);
				throw;
			}
			return token;
		}
	}
	constexpr void give(Token&& token) { _container.push_back(::std::move(token)); }
};

// 带资源的 semaphore：许可就是容器里的某个 token。acquire() 完成时交出一个
// permit（持有 token，析构时归还）。排队、取消、归还时直接交给队首等待者
// 都由 acquire_core 完成，这里只负责构造和对外接口。
template <resource_token Token, available_container<Token> AvailableContainer>
struct resource_pool
{
private:
	using self_type = resource_pool;
	using store_type = container_store<Token, AvailableContainer>;
	using core_type = acquire_core<store_type>;
public:
	using token_type = Token;
	using available_container_type = AvailableContainer;
	using permit = typename core_type::permit_type;
	using acquire_sender = typename core_type::acquire_sender;

	core_type _core;

	constexpr resource_pool() noexcept requires ::std::default_initializable<available_container_type>
		: _core(::std::in_place)
	{
	}
	explicit constexpr resource_pool(available_container_type&& container) noexcept
		: _core(::std::in_place, ::std::move(container))
	{
	}
	explicit constexpr resource_pool(available_container_type const& container) noexcept
		requires ::std::copy_constructible<available_container_type>
		: _core(::std::in_place, container)
	{
	}
	constexpr explicit resource_pool(::std::in_place_t, auto&&... args) noexcept
		requires ::std::constructible_from<available_container_type, decltype(args)...>
		: _core(::std::in_place, ::std::forward<decltype(args)>(args)...)
	{
	}

	constexpr resource_pool(self_type&&) noexcept = delete;
	constexpr resource_pool(self_type const&) noexcept = delete;
	constexpr self_type& operator=(self_type&&) noexcept = delete;
	constexpr self_type& operator=(self_type const&) noexcept = delete;

	[[nodiscard]] constexpr acquire_sender acquire() noexcept { return _core.acquire(); }
	[[nodiscard]] ::std::optional<permit> try_acquire()
	{
		auto token = _core._try_take();
		if (!token)
			return ::std::nullopt;
		return ::std::optional<permit>{ ::std::in_place, _core, ::std::move(*token) };
	}
};

NAGISA_BUILD_LIB_DETAIL_END
