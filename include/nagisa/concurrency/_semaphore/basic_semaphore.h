#pragma once

#include "./wait_queue.h"
#include "./environment.h"

#include "./macro_push.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

struct semaphore_waiter
{
	semaphore_waiter* _next = nullptr;
	void (*_complete)(semaphore_waiter*) noexcept = nullptr;
	// 下面两个字段仅在持有 basic_semaphore::_mutex 时读写，用来仲裁
	// "入队前就已经被取消" 这一条路径，不参与 _release 摘节点之后的任何决策
	// （那条路径上节点可能已经失效，不能再碰）。
	bool _enqueued = false;
	bool _cancelled_before_enqueue = false;
};

enum class acquire_result
{
	acquired,
	queued,
	cancelled,
};

template <::std::ptrdiff_t Max = (::std::numeric_limits<::std::ptrdiff_t>::max)()>
struct basic_semaphore
{
private:
	using self_type = basic_semaphore;
public:
	static constexpr ::std::ptrdiff_t max() noexcept { return Max; }

	// ---------------------------------------------------------------- sender

	struct acquire_sender
	{
		using sender_concept = ::stdexec::sender_t;
		using completion_signatures = ::stdexec::completion_signatures<
			::stdexec::set_value_t(),
			::stdexec::set_stopped_t()>;
		self_type* _semaphore;

		template <class ReceiverType>
		struct operation : semaphore_waiter
		{
			using operation_state_concept = ::stdexec::operation_state_t;
			using receiver_type = ReceiverType;
			using stop_token_type = ::stdexec::stop_token_of_t<::stdexec::env_of_t<receiver_type>>;
			struct on_stop
			{
				operation* _operation;
				constexpr auto operator()() const noexcept -> void
				{
					if (!_operation->_semaphore->_cancel(*_operation))
						return;
					_operation->_on_stop.reset();
					::stdexec::set_stopped(::std::move(_operation->_receiver));
				}
			};
			using callback_type = typename stop_token_type::template callback_type<on_stop>;
			static constexpr bool _uses_stop_callback = !::stdexec::unstoppable_token<stop_token_type>;
			using slot_type = ::std::conditional_t<_uses_stop_callback, ::std::optional<callback_type>, no_stop_callback>;

			self_type* _semaphore;
			receiver_type _receiver;
#ifdef _MSC_VER
			[[msvc::no_unique_address]]
#else
			[[no_unique_address]]
#endif
			slot_type _on_stop{};

			constexpr operation(self_type& semaphore, receiver_type receiver) noexcept
				: _semaphore(&semaphore)
				, _receiver(::std::move(receiver))
			{
				semaphore_waiter::_complete = [](semaphore_waiter* self) noexcept
					{
						auto target = static_cast<operation*>(self);
						if constexpr (_uses_stop_callback)
							target->_on_stop.reset();
						::stdexec::set_value(::std::move(target->_receiver));
					};
			}
			constexpr operation(operation&&) = delete;
			constexpr operation(operation const&) = delete;
			constexpr operation& operator=(operation const&) = delete;
			constexpr operation& operator=(operation&&) = delete;
			constexpr void start() & noexcept
			{
				// 先在节点还是线程私有状态时订阅取消——这一步可能同步重入
				// on_stop（token 已经请求停止）。节点此刻还没发布给任何人，
				// on_stop 只能把结果记在 _cancelled_before_enqueue 上，不会和
				// _release 竞争同一个节点。
				if constexpr (_uses_stop_callback)
					_on_stop.emplace(::stdexec::get_stop_token(::stdexec::get_env(_receiver)), on_stop{ this });
				// 从这里开始才可能被其它线程看到；入队、提前取消、立即可用
				// 这三种结果在同一次加锁里仲裁完，start() 之后不会再碰 *this。
				switch (_semaphore->_acquire_or_enqueue(*this))
				{
				case acquire_result::acquired:
					::stdexec::set_value(::std::move(_receiver));
					return;
				case acquire_result::queued:
					return;
				case acquire_result::cancelled:
					if constexpr (_uses_stop_callback)
						_on_stop.reset();
					::stdexec::set_stopped(::std::move(_receiver));
					return;
				}
			}
		};

		template <::stdexec::receiver ReceiverType>
		[[nodiscard]] constexpr auto connect(ReceiverType receiver) const -> operation<ReceiverType>
		{
			return operation<ReceiverType>{*_semaphore, ::std::move(receiver)};
		}
	};

	mutable ::std::mutex _mutex{};
	::std::ptrdiff_t _count;
	wait_queue<semaphore_waiter> _waiters{};

	constexpr explicit basic_semaphore(::std::ptrdiff_t desired) noexcept
		: _count(desired)
	{
	}

	constexpr basic_semaphore(self_type&&) noexcept = delete;
	constexpr basic_semaphore(self_type const&) noexcept = delete;
	constexpr self_type& operator=(self_type&&) noexcept = delete;
	constexpr self_type& operator=(self_type const&) noexcept = delete;

	constexpr ~basic_semaphore() noexcept
	{
		NAGISA_CONCURRENCY_SEMAPHORE_ASSERT(_waiters.empty());
	}

	[[nodiscard]] constexpr acquire_sender acquire() noexcept { return acquire_sender{ this }; }
	[[nodiscard]] bool try_acquire() noexcept
	{
		auto guard = ::std::scoped_lock{ _mutex };
		if (_count <= 0)
			return false;
		--_count;
		return true;
	}
	void release(::std::ptrdiff_t update = 1) noexcept
	{
		for (; update != 0; --update)
			self_type::_release();
	}
	[[nodiscard]] acquire_result _acquire_or_enqueue(semaphore_waiter& target) noexcept
	{
		auto guard = ::std::scoped_lock{ _mutex };
		if (target._cancelled_before_enqueue)
			return acquire_result::cancelled;
		if (_count > 0)
		{
			--_count;
			return acquire_result::acquired;
		}
		_waiters.push_back(target);
		target._enqueued = true;
		return acquire_result::queued;
	}
	// 返回 true 表示这次调用赢得了完成该节点的权利（调用方应 set_stopped）。
	// 节点还没入队（start() 还没调用 _acquire_or_enqueue）时，不能去碰它在
	// 等待队列里的链接——只记下"已取消"，真正的完成交给 _acquire_or_enqueue
	// 在同一把锁里自己发现并处理。
	[[nodiscard]] bool _cancel(semaphore_waiter& target) noexcept
	{
		auto guard = ::std::scoped_lock{ _mutex };
		if (!target._enqueued)
		{
			target._cancelled_before_enqueue = true;
			return false;
		}
		return _waiters.unlink(&target);
	}
	void _release() noexcept
	{
		auto next = static_cast<semaphore_waiter*>(nullptr);
		{
			auto guard = ::std::scoped_lock{ _mutex };
			next = _waiters.pop_front();
			if (next == nullptr)
			{
				++_count;
				return;
			}
		}
		next->_complete(next);
	}
};

NAGISA_BUILD_LIB_DETAIL_END

#include "./macro_pop.h"
