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
	// 下面两个字段仅在持有 lease_pool::_mutex 时读写，用来仲裁
	// "入队前就已经被取消" 这一条路径，不参与 _put 摘节点之后的任何决策
	// （那条路径上节点可能已经失效，不能再碰）。
	bool _enqueued = false;
	bool _cancelled_before_enqueue = false;
};

enum class lease_acquire_result
{
	acquired,
	queued,
	cancelled,
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

	struct acquire_attempt
	{
		lease_acquire_result result;
		::std::optional<token_type> token{};
	};

	struct lease
	{
		self_type* _pool = nullptr;
		token_type _token{};

		constexpr lease() = default;
		constexpr lease(self_type& pool, token_type token) noexcept
			: _pool(::std::addressof(pool))
			, _token(::std::move(token))
		{
		}
		constexpr lease(lease&& other) noexcept
			: _pool(::std::exchange(other._pool, nullptr))
			, _token(::std::exchange(other._token, token_type{}))
		{
		}
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
					if (!_operation->_pool->_cancel(*_operation))
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
			constexpr operation& operator=(operation const&) = delete;
			constexpr operation& operator=(operation&&) = delete;
			constexpr void start() & noexcept
			{
				// 先在节点还是线程私有状态时订阅取消——这一步可能同步重入
				// on_stop（令牌已经请求停止）。节点此刻还没发布给任何人，
				// on_stop 只能把结果记在 _cancelled_before_enqueue 上，不会和
				// _put 竞争同一个节点。
				if constexpr (_uses_stop_callback)
					_on_stop.emplace(::stdexec::get_stop_token(::stdexec::get_env(_receiver)), on_stop{ this });
				// 从这里开始才可能被其它线程看到；入队、提前取消、立即可用
				// 这三种结果在同一次加锁里仲裁完，start() 之后不会再碰 *this。
				auto attempt = _pool->_acquire_or_enqueue(*this);
				switch (attempt.result)
				{
				case lease_acquire_result::acquired:
					::stdexec::set_value(::std::move(_receiver), lease{ *_pool, ::std::move(*attempt.token) });
					return;
				case lease_acquire_result::queued:
					return;
				case lease_acquire_result::cancelled:
					if constexpr (_uses_stop_callback)
						_on_stop.reset();
					::stdexec::set_stopped(::std::move(_receiver));
					return;
				}
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
	wait_queue<waiter_type> _waiters{};

	constexpr lease_pool() noexcept requires ::std::default_initializable<available_container_type> = default;
	explicit constexpr lease_pool(available_container_type&& container) noexcept
		: _available_container(::std::move(container))
	{
	}
	explicit constexpr lease_pool(available_container_type const& container) noexcept
		requires ::std::copy_constructible<available_container_type>
		: _available_container(container)
	{
	}
	constexpr explicit lease_pool(::std::in_place_t, auto&&... args) noexcept
		requires ::std::constructible_from<available_container_type, decltype(args)...>
		: _available_container(::std::forward<decltype(args)>(args)...)
	{
	}

	constexpr lease_pool(self_type&&) noexcept = delete;
	constexpr lease_pool(self_type const&) noexcept = delete;
	constexpr self_type& operator=(self_type&&) noexcept = delete;
	constexpr self_type& operator=(self_type const&) noexcept = delete;

	constexpr ~lease_pool() noexcept
	{
		NAGISA_CONCURRENCY_LEASE_ASSERT(_waiters.empty());
	}
	[[nodiscard]] constexpr acquire_sender acquire() noexcept { return acquire_sender{ this }; }
	[[nodiscard]] ::std::optional<lease> try_acquire()
	{
		auto guard = ::std::scoped_lock{ _mutex };
		if (::std::ranges::empty(_available_container))
			return ::std::nullopt;
		return ::std::optional<lease>{::std::in_place, * this, self_type::_extract_available()};
	}
	[[nodiscard]] token_type _extract_available()
	{
		token_type token = ::std::move(_available_container.back());
		if constexpr (noexcept(_available_container.pop_back()))
		{
			_available_container.pop_back();
			return token;
		}
		else
		{
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
	}
	[[nodiscard]] acquire_attempt _acquire_or_enqueue(waiter_type& target)
	{
		auto guard = ::std::scoped_lock{ _mutex };
		if (target._cancelled_before_enqueue)
			return acquire_attempt{ lease_acquire_result::cancelled };
		if (!::std::ranges::empty(_available_container))
			return acquire_attempt{ lease_acquire_result::acquired, self_type::_extract_available() };
		_waiters.push_back(target);
		target._enqueued = true;
		return acquire_attempt{ lease_acquire_result::queued };
	}
	// 返回 true 表示这次调用赢得了完成该节点的权利（调用方应 set_stopped）。
	// 节点还没入队（start() 还没调用 _acquire_or_enqueue）时，不能去碰它在
	// 等待队列里的链接——只记下"已取消"，真正的完成交给 _acquire_or_enqueue
	// 在同一把锁里自己发现并处理。
	[[nodiscard]] bool _cancel(waiter_type& target)
	{
		auto guard = ::std::scoped_lock{ _mutex };
		if (!target._enqueued)
		{
			target._cancelled_before_enqueue = true;
			return false;
		}
		return _waiters.unlink(&target);
	}
	void _put(token_type token)
	{
		auto next = static_cast<waiter_type*>(nullptr);
		{
			auto guard = ::std::scoped_lock{ _mutex };
			next = _waiters.pop_front();
			if (next == nullptr)
			{
				_available_container.push_back(::std::move(token));
				return;
			}
		}
		next->_complete(next, ::std::move(token));
	}
};

NAGISA_BUILD_LIB_DETAIL_END

#include "./macro_pop.h"