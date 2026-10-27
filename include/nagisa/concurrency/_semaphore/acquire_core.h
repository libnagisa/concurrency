#pragma once

#include "./wait_queue.h"
#include "./resource_permit.h"
#include "./give_n.h"

#include "./environment.h"

#include "./macro_push.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

// 等待节点。Values 是 Store 取出的值；为空表示 Store 不带值（计数器），此时
// 完成函数不收参数，节点里也没有任何给值预留的东西。
// 节点只有排队用的两个指针；"入队前就被取消" 的仲裁状态不在节点里，而是放在
// 可取消的 operation 里（见 cancel_state），不可取消的 token 不付这份开销。
template <class... Values>
struct core_waiter
{
	core_waiter* _next = nullptr;
	void (*_complete)(core_waiter*, Values...) noexcept = nullptr;
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

// 仅在持有 acquire_core::_mutex 时读写，用来仲裁 "入队前就已经被取消" 这一条
// 路径，不参与 _release 摘节点之后的任何决策（那条路径上节点可能已经失效，
// 不能再碰）。enqueued 与 cancelled 互斥，所以一个字节就够。
enum class wait_phase : unsigned char
{
	idle,
	enqueued,
	cancelled,
};
struct cancel_state
{
	wait_phase _phase = wait_phase::idle;
};
struct no_cancel_state {};

// semaphore 与 resource_pool 共用的核心：一把锁、一个 Store、一条等待队列，
// 以及 "订阅取消 -> 加锁仲裁 -> 完成" 这套 sender 状态机。
//
// Store 只回答下面几件事（都在 _mutex 内调用）：
//   value_type          取出的值的类型；void 表示不带值
//   bool available()    现在有没有可取的
//   take()              取一个（value_type 非 void 时返回它）
//   give([value])       还一个（value_type 非 void 时带上值）
// 不带值的 Store 另可提供 give(count)：一次还 count 个。可选，没有就由 give_n
// 退化成循环调用 give() 手拼（见 give_n.h）。

// value_type 为 void（计数器）：acquire() 以 set_value() 完成，没有任何载荷；
// value_type 非 void（容器）：acquire() 以 set_value(resource_permit) 完成，
// permit 析构时调用 _release(value) 归还。
// 归还时若有等待者，值不经过 Store，直接交给队首——与手写的单锁实现一致。
//
// 不带值的 Store 另有 acquire_permit()：以 set_value(counter_permit) 完成，
// permit 析构时自动 _release()。semaphore_guard 直接用它，不必再用 then 在
// sender 外面套一层（那会多出一个捕获 this 的 lambda 和一层 receiver）。
template <class Store>
struct acquire_core
{
private:
	using self_type = acquire_core;
public:
	using store_type = Store;
	using value_type = typename Store::value_type;
	using waiter_type = ::std::conditional_t<::std::is_void_v<value_type>, core_waiter<>, core_waiter<value_type>>;
	using attempt_type = acquire_attempt<value_type>;

	static constexpr bool _has_value = !::std::is_void_v<value_type>;

	using permit_type = ::std::conditional_t<_has_value, resource_permit<self_type>, counter_permit<self_type>>;

	// ---------------------------------------------------------------- sender

	// WithPermit 为 false 只可能出现在不带值的 Store 上（见 acquire()）。
	template <bool WithPermit>
	struct basic_acquire_sender
	{
		static constexpr bool _has_permit = WithPermit;
		using sender_concept = ::stdexec::sender_t;
		using completion_signatures = ::std::conditional_t<_has_permit,
			::stdexec::completion_signatures<::stdexec::set_value_t(permit_type), ::stdexec::set_stopped_t()>,
			::stdexec::completion_signatures<::stdexec::set_value_t(), ::stdexec::set_stopped_t()>>;
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
					if (!_operation->_core->_cancel(*_operation, _operation->_state))
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
			// 只有可取消的 token 才需要 "入队前就被取消" 的仲裁状态。
#ifdef _MSC_VER
			[[msvc::no_unique_address]]
#else
			[[no_unique_address]]
#endif
			::std::conditional_t<_uses_stop_callback, cancel_state, no_cancel_state> _state{};

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
							if constexpr (_has_permit)
								::stdexec::set_value(::std::move(target->_receiver), permit_type{ *target->_core });
							else
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
				// on_stop 只能把结果记在 _state 上，不会和
				// _release 竞争同一个节点。
				if constexpr (_uses_stop_callback)
					_on_stop.emplace(::stdexec::get_stop_token(::stdexec::get_env(_receiver)), on_stop{ this });
				// 从这里开始才可能被其它线程看到；入队、提前取消、立即可用
				// 这三种结果在同一次加锁里仲裁完，start() 之后不会再碰 *this。
				auto attempt = _core->_acquire_or_enqueue(*this, _state);
				switch (attempt.result)
				{
				case acquire_result::acquired:
					if constexpr (_has_value)
						::stdexec::set_value(::std::move(_receiver), permit_type{ *_core, ::std::move(*attempt.value) });
					else if constexpr (_has_permit)
						::stdexec::set_value(::std::move(_receiver), permit_type{ *_core });
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

	using acquire_sender = basic_acquire_sender<_has_value>;
	using permit_sender = basic_acquire_sender<true>;

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
	[[nodiscard]] constexpr permit_sender acquire_permit() noexcept requires (!_has_value) { return permit_sender{ this }; }

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
	template <class State>
	[[nodiscard]] attempt_type _acquire_or_enqueue(waiter_type& target, State& state)
	{
		auto guard = ::std::scoped_lock{ _mutex };
		if constexpr (::std::same_as<State, cancel_state>)
		{
			if (state._phase == wait_phase::cancelled)
				return attempt_type{ acquire_result::cancelled };
		}
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
		if constexpr (::std::same_as<State, cancel_state>)
			state._phase = wait_phase::enqueued;
		return attempt_type{ acquire_result::queued };
	}
	// 返回 true 表示这次调用赢得了完成该节点的权利（调用方应 set_stopped）。
	// 节点还没入队（start() 还没调用 _acquire_or_enqueue）时，不能去碰它在
	// 等待队列里的链接——只记下"已取消"，真正的完成交给 _acquire_or_enqueue
	// 在同一把锁里自己发现并处理。
	[[nodiscard]] bool _cancel(waiter_type& target, cancel_state& state)
	{
		auto guard = ::std::scoped_lock{ _mutex };
		if (state._phase != wait_phase::enqueued)
		{
			state._phase = wait_phase::cancelled;
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
	// 计数器专用的批量归还：只加一次锁，按 FIFO 摘下最多 update 个等待者，
	// 剩下的许可一次记回计数（give_n：Store 有 give(count) 就是 O(1)），然后在锁外
	// 依次唤醒。多次单个 _release 要加 update 次锁，并且会让别的线程插进两次归还之间。
	// update 为负是前置条件违反。
	void _release_n(::std::ptrdiff_t update) requires (!_has_value)
	{
		NAGISA_CONCURRENCY_SEMAPHORE_ASSERT(update >= 0);
		if (update == 0)
			return;
		auto head = static_cast<waiter_type*>(nullptr);
		auto tail = &head;
		{
			auto guard = ::std::scoped_lock{ _mutex };
			for (; update != 0; --update)
			{
				auto next = _waiters.pop_front();
				if (next == nullptr)
				{
					give_n(_store, update);
					break;
				}
				// 节点已经离开等待队列，_next 可以直接拿来串成本地链表。
				*tail = next;
				tail = &next->_next;
			}
			*tail = nullptr;
		}
		// 完成函数可能销毁节点，所以先取下一个再调用。
		for (auto node = head; node != nullptr;)
		{
			auto next = node->_next;
			node->_complete(node);
			node = next;
		}
	}
};

NAGISA_BUILD_LIB_DETAIL_END

#include "./macro_pop.h"
