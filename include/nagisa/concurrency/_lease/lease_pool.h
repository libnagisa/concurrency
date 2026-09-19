#pragma once

#include "./token.h"
#include "./available_container.h"

#include "./environment.h"

#include "./macro_push.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

template<lease_token Token>
struct waiter
{
	using token_type = Token;

	waiter* _next = nullptr;
	void (*_complete)(waiter*, token_type) noexcept = nullptr;
};

template <lease_token Token, available_container<Token> AvailableContainer>
struct lease_pool
{
private:
	using self_type = lease_pool;
public:
	using token_type = Token;
	using available_container_type = AvailableContainer;
	using waiter_type = waiter<token_type>;

	struct lease
	{
		self_type* _pool = nullptr;
		token_type _token{};

		constexpr lease() = default;
		constexpr lease(self_type& pool, token_type token) noexcept
			: _pool(::std::addressof(pool))
			, _token(::std::move(token))
		{}
		constexpr lease(lease&& other) noexcept
			: _pool(::std::exchange(other._pool, nullptr))
			, _token(::std::exchange(other._token, token_type{}))
		{}
		constexpr lease& operator=(lease&& other) noexcept
		{
			auto discarded = ::std::move(*this);
			_pool = ::std::exchange(other._pool, nullptr);
			_token = ::std::exchange(other._token, token_type{});
			return *this;
		}
		constexpr lease(lease const&) noexcept = delete;
		constexpr lease& operator=(lease const&) noexcept = delete;
		constexpr ~lease()
		{
			if (_pool != nullptr)
				_pool->_put(::std::move(_token));
		}

		[[nodiscard]] constexpr explicit operator bool() const noexcept { return _pool != nullptr; }
		[[nodiscard]] constexpr auto&& token() noexcept { return _token; }
		[[nodiscard]] constexpr auto&& token() const noexcept { return _token; }
	};

	// ---------------------------------------------------------------- sender

	struct acquire_sender
	{
		using sender_concept = ::stdexec::sender_t;
		using completion_signatures = ::stdexec::completion_signatures<
			::stdexec::set_value_t(lease),
			::stdexec::set_stopped_t()>;
		self_type* _pool;

		template <class ReceiverType>
		struct operation : waiter_type
		{
			using operation_state_concept = ::stdexec::operation_state_t;
			using receiver_type = ReceiverType;
			using stop_token_type = ::stdexec::stop_token_of_t<::stdexec::env_of_t<receiver_type>>;
			struct on_stop
			{
				operation* _operation;
				constexpr auto operator()() const noexcept -> void
				{
					if (!_operation->_pool->_unlink(_operation))
						return;
					// 从回调内部销毁这个回调本身是允许的（`__removed_during_callback_`），
					// upstream 的 `when_all` 也是这么干的。
					_operation->_on_stop.reset();
					::stdexec::set_stopped(::std::move(_operation->_receiver));
				}
			};
			using callback_type = typename stop_token_type::template callback_type<on_stop>;
			static constexpr bool _uses_stop_callback = !::stdexec::unstoppable_token<stop_token_type>;
			using slot_type = ::std::conditional_t<_uses_stop_callback, ::std::optional<callback_type>, no_stop_callback>;

			self_type* _pool;
			receiver_type _receiver;
#ifdef _MSC_VER
			[[msvc::no_unique_address]]
#else
			[[no_unique_address]]
#endif
			slot_type _on_stop{};

			constexpr operation(self_type& pool, receiver_type receiver) noexcept
				: _pool(&pool)
				, _receiver(::std::move(receiver))
			{
				waiter_type::_complete = [](waiter_type* self, token_type token) noexcept
					{
						auto target = static_cast<operation*>(self);
						if constexpr (_uses_stop_callback)
						{
							// 先注销：`~inplace_stop_callback` 会等正在跑的取消回调返回，
							// 那个回调发现自己已不在队列里就直接退出，所以不会互等。
							target->_on_stop.reset();
						}
						::stdexec::set_value(::std::move(target->_receiver), lease{ *target->_pool,	::std::move(token) });
					};
			}
			constexpr operation(operation&&) = delete;
			constexpr operation(operation const&) = delete;
			constexpr operation& operator=(operation const&)  = delete;
			constexpr operation& operator=(operation&&)  = delete;
			constexpr void start() & noexcept
			{
				if (auto token = _pool->_take_or_enqueue(*this))
				{
					::stdexec::set_value(::std::move(_receiver), lease{ *_pool, ::std::move(*token) });
					return;
				}
				// 排上队了。装取消回调——装的过程里回调就可能立刻跑（令牌已停止），
				// 那条路会把自己摘掉并完成，所以之后不能再碰 *this。
				if constexpr (_uses_stop_callback)
					_on_stop.emplace(::stdexec::get_stop_token(::stdexec::get_env(_receiver)), on_stop{ this });
			}
		};

		template<::stdexec::receiver ReceiverType>
		[[nodiscard]] constexpr auto connect(ReceiverType receiver) const -> operation<ReceiverType>
		{
			return operation<ReceiverType>{*_pool, ::std::move(receiver)};
		}
	};

	mutable ::std::mutex _mutex{};
	available_container_type _available_container{};
	waiter_type* _head = nullptr;
	waiter_type* _tail = nullptr;
	::std::size_t _supplied = 0;

	constexpr lease_pool() noexcept requires ::std::default_initializable<available_container_type> = default;
	explicit constexpr lease_pool(available_container_type&& container) noexcept
		: _available_container(::std::move(container))
		, _supplied(_available_container.size())
	{}
	explicit constexpr lease_pool(available_container_type const& container) noexcept 
		requires ::std::copy_constructible<available_container_type>
		: _available_container(container)
		, _supplied(container.size()) 
	{}
	constexpr explicit lease_pool(::std::in_place_t, auto&&... args) noexcept
		requires ::std::constructible_from<available_container_type, decltype(args)...>
		: _available_container(::std::forward<decltype(args)>(args)...)
		, _supplied(_available_container.size()) 
	{}

	constexpr lease_pool(self_type&&) noexcept = delete;
	constexpr lease_pool(self_type const&) noexcept = delete;
	constexpr self_type& operator=(self_type&&) noexcept = delete;
	constexpr self_type& operator=(self_type const&) noexcept = delete;

	constexpr ~lease_pool() noexcept
	{
		NAGISA_CONCURRENCY_LEASE_ASSERT(_head == nullptr);
		NAGISA_CONCURRENCY_LEASE_ASSERT(_available_container.size() == _supplied);
	}
	[[nodiscard]] constexpr acquire_sender acquire() noexcept { return acquire_sender{ this }; }
	[[nodiscard]] ::std::optional<lease> try_acquire()
	{
		auto guard = ::std::scoped_lock{ _mutex };
		if (_available_container.empty())
			return ::std::nullopt;
		return ::std::optional<lease>{::std::in_place, *this, self_type::_extract_available()};
	}
	[[nodiscard]] auto size() const
	{
		auto guard = ::std::scoped_lock{ _mutex };
		return _supplied;
	}
	[[nodiscard]] auto available() const
	{
		auto guard = ::std::scoped_lock{ _mutex };
		return _available_container.size();
	}
	void supply(Token value)
	{
		self_type::_put(::std::move(value));
		auto guard = ::std::scoped_lock{ _mutex };
		++_supplied;
	}
	[[nodiscard]] token_type _extract_available()
	{
		auto token = ::std::move(_available_container.back());
		try
		{
			_available_container.pop_back();
		}
		catch (...)
		{
			_available_container.back() = ::std::move(token);
			throw;
		}
		return token;
	}
	[[nodiscard]] ::std::optional<token_type> _take_or_enqueue(waiter_type& target)
	{
		auto guard = ::std::scoped_lock{ _mutex };
		if (!_available_container.empty())
			return self_type::_extract_available();
		target._next = nullptr;
		if (_tail == nullptr)
			_head = &target;
		else
			_tail->_next = &target;
		_tail = &target;
		return ::std::nullopt;
	}
	[[nodiscard]] bool _unlink(waiter_type* target)
	{
		auto guard = ::std::scoped_lock{ _mutex };
		auto link = &_head;
		auto previous = static_cast<waiter_type*>(nullptr);
		while (*link != nullptr)
		{
			if (*link == target)
			{
				*link = target->_next;
				if (_tail == target)
					_tail = previous;
				return true;
			}
			previous = *link;
			link = &(*link)->_next;
		}
		return false;
	}
	void _put(token_type token)
	{
		auto next = static_cast<waiter_type*>(nullptr);
		{
			auto guard = ::std::scoped_lock{ _mutex };
			if (_head == nullptr)
			{
				_available_container.push_back(::std::move(token));
				return;
			}
			next = _head;
			_head = next->_next;
			if (_head == nullptr)
				_tail = nullptr;
		}
		next->_complete(next, ::std::move(token));
	}
};

NAGISA_BUILD_LIB_DETAIL_END

#include "./macro_pop.h"