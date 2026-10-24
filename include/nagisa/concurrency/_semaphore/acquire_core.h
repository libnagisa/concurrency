#pragma once

#include "./wait_queue.h"
#include "./resource_permit.h"

#include "./environment.h"

#include "./macro_push.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

// 下面两个字段仅在持有 acquire_core::_mutex 时读写，用来仲裁
// "入队前就已经被取消" 这一条路径，不参与 _release 摘节点之后的任何决策
// （那条路径上节点可能已经失效，不能再碰）。
struct waiter_state
{
	bool _enqueued = false;
	bool _cancelled_before_enqueue = false;
};

// 等待节点。Value 是 Store 取出的值；void 表示 Store 不带值（计数器），此时
// 完成函数不收参数，节点里也没有任何给值预留的东西。
template <class Value>
struct core_waiter : waiter_state
{
	core_waiter* _next = nullptr;
	void (*_complete)(core_waiter*, Value) noexcept = nullptr;
};
template <>
struct core_waiter<void> : waiter_state
{
	core_waiter* _next = nullptr;
	void (*_complete)(core_waiter*) noexcept = nullptr;
};

enum class acquire_result
{
	acquired,
	queued,
	cancelled,
};

template <class Value>
struct acquire_attempt
{
	acquire_result result;
	::std::optional<Value> value{};
};
template <>
struct acquire_attempt<void>
{
	acquire_result result;
};

// semaphore 与 resource_pool 共用的核心：一把锁、一个 Store、一条等待队列，
// 以及 "订阅取消 -> 加锁仲裁 -> 完成" 这套 sender 状态机。
//
// Store 只回答三件事（都在 _mutex 内调用）：
//   value_type          取出的值的类型；void 表示不带值
//   bool available()    现在有没有可取的
//   take()              取一个（value_type 非 void 时返回它）
//   give([value])       还一个（value_type 非 void 时带上值）
//
// value_type 为 void（计数器）：acquire() 以 set_value() 完成，没有任何载荷；
// value_type 非 void（容器）：acquire() 以 set_value(resource_permit) 完成，
// permit 析构时调用 _release(value) 归还。
// 归还时若有等待者，值不经过 Store，直接交给队首——与手写的单锁实现一致。
template <class Store>
struct acquire_core
{
private:
	using self_type = acquire_core;
public:
	using store_type = Store;
	using value_type = typename Store::value_type;
	using waiter_type = core_waiter<value_type>;
	using attempt_type = acquire_attempt<value_type>;

	static constexpr bool _has_value = !::std::is_void_v<value_type>;

	using permit_type = resource_permit<self_type>;
	using _signatures = ::std::conditional_t<_has_value,
		::stdexec::completion_signatures<::stdexec::set_value_t(permit_type), ::stdexec::set_stopped_t()>,
		::stdexec::completion_signatures<::stdexec::set_value_t(), ::stdexec::set_stopped_t()>>;

	// ---------------------------------------------------------------- sender

	struct acquire_sender
	{
		using sender_concept = ::stdexec::sender_t;
		using completion_signatures = _signatures;
		self_type* _core;

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
					if (!_operation->_core->_cancel(*_operation))
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

			self_type* _core;
			receiver_type _receiver;
#ifdef _MSC_VER
			[[msvc::no_unique_address]]
#else
			[[no_unique_address]]
#endif
			slot_type _on_stop{};

			constexpr operation(self_type& core, receiver_type receiver) noexcept
				: _core(&core)
				, _receiver(::std::move(receiver))
			{
				if constexpr (_has_value)
					waiter_type::_complete = [](waiter_type* self, value_type value) noexcept
						{
							auto target = static_cast<operation*>(self);
							if constexpr (_uses_stop_callback)
							{
								// 先注销：`~inplace_stop_callback` 会等正在跑的取消回调返回，
								// 那个回调发现自己已不在队列里就直接退出，所以不会互等。
								target->_on_stop.reset();
							}
							::stdexec::set_value(::std::move(target->_receiver), permit_type{ *target->_core, ::std::move(value) });
						};
				else
					waiter_type::_complete = [](waiter_type* self) noexcept
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
				auto attempt = _core->_acquire_or_enqueue(*this);
				switch (attempt.result)
				{
				case acquire_result::acquired:
					if constexpr (_has_value)
						::stdexec::set_value(::std::move(_receiver), permit_type{ *_core, ::std::move(*attempt.value) });
					else
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
			return operation<ReceiverType>{ *_core, ::std::move(receiver) };
		}
	};

	mutable ::std::mutex _mutex{};
	store_type _store;
	wait_queue<waiter_type> _waiters{};

	template <class... Args>
		requires ::std::constructible_from<store_type, Args...>
	constexpr explicit acquire_core(::std::in_place_t, Args&&... args) noexcept
		: _store(::std::forward<Args>(args)...)
	{
	}

	constexpr acquire_core(self_type&&) noexcept = delete;
	constexpr acquire_core(self_type const&) noexcept = delete;
	constexpr self_type& operator=(self_type&&) noexcept = delete;
	constexpr self_type& operator=(self_type const&) noexcept = delete;

	constexpr ~acquire_core() noexcept
	{
		NAGISA_CONCURRENCY_SEMAPHORE_ASSERT(_waiters.empty());
	}

	[[nodiscard]] constexpr acquire_sender acquire() noexcept { return acquire_sender{ this }; }

	// 不排队地取一个：Store 不带值时返回 bool，否则返回 optional<value_type>。
	[[nodiscard]] auto _try_take()
	{
		auto guard = ::std::scoped_lock{ _mutex };
		if constexpr (_has_value)
		{
			if (!_store.available())
				return ::std::optional<value_type>{};
			return ::std::optional<value_type>{ _store.take() };
		}
		else
		{
			if (!_store.available())
				return false;
			_store.take();
			return true;
		}
	}
	[[nodiscard]] attempt_type _acquire_or_enqueue(waiter_type& target)
	{
		auto guard = ::std::scoped_lock{ _mutex };
		if (target._cancelled_before_enqueue)
			return attempt_type{ acquire_result::cancelled };
		if (_store.available())
		{
			if constexpr (_has_value)
				return attempt_type{ acquire_result::acquired, _store.take() };
			else
			{
				_store.take();
				return attempt_type{ acquire_result::acquired };
			}
		}
		_waiters.push_back(target);
		target._enqueued = true;
		return attempt_type{ acquire_result::queued };
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
	// 归还一个：有等待者就在锁外直接交给队首，否则放回 Store。
	template <class... Values>
		requires (sizeof...(Values) == (_has_value ? 1 : 0))
	void _release(Values&&... values)
	{
		auto next = static_cast<waiter_type*>(nullptr);
		{
			auto guard = ::std::scoped_lock{ _mutex };
			next = _waiters.pop_front();
			if (next == nullptr)
			{
				_store.give(::std::forward<Values>(values)...);
				return;
			}
		}
		next->_complete(next, ::std::forward<Values>(values)...);
	}
};

NAGISA_BUILD_LIB_DETAIL_END

#include "./macro_pop.h"
