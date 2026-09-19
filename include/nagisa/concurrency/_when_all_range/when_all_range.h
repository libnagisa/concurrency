#pragma once

#include "./manual_lifetime.h"
#include "./completion_kind.h"

#include "./environment.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

template <class ReceiverType>
struct when_all_range_state
{
	using receiver_type = ReceiverType;
	struct forward_stop
	{
		when_all_range_state* _state;
		auto operator()() const noexcept -> void
		{
			// 回调执行期间多握一枚计数：`request_stop` 可能让孩子同步完成、计数归零，
			// 进而 `_on_stop.reset()` 把我们正待在里面的这个回调销毁掉。
			//
			// **只在计数还没归零时才握**。直接 `fetch_add` 会把已经归零的计数顶回 1，
			// 下面自己的 `_arrive()` 再减回 0，于是本回调也判定"我是最后一个" ——
			// 和那个刚刚释放屏障的线程同时跑完成路径：同一个 `_on_stop` 被析构两次、
			// 同一个 receiver 被完成两次。实测第 1 轮就中（tmp/rev/race.cpp）。
			//
			// 归零就直接退出是安全的：本回调还在跑，说明那个线程正卡在
			// `~inplace_stop_callback` 里等我们返回，状态一定还活着；完成这件事
			// 交给它一个人做。
			auto expected = _state->_pending.load(::std::memory_order_relaxed);
			do
			{
				if (expected == 0)
					return;
			} while (!_state->_pending.compare_exchange_weak(
				expected, expected + 1, ::std::memory_order_acq_rel, ::std::memory_order_relaxed));
			_state->_stopped();
			_state->_arrive();
		}
	};
	using outer_token_type = ::stdexec::stop_token_of_t<::stdexec::env_of_t<receiver_type>>;
	using stop_callback_type = typename outer_token_type::template callback_type<forward_stop>;
	static constexpr bool _uses_stop_callback = !::stdexec::unstoppable_token<outer_token_type>;
	using on_stop_type = ::std::conditional_t<_uses_stop_callback, ::std::optional<stop_callback_type>, no_stop_callback>;

	receiver_type _receiver;
	::std::size_t _count;
	::std::atomic<::std::size_t> _pending;
	::std::atomic<completion_kind> _kind{ completion_kind::value };
	::std::exception_ptr _error{};
	::stdexec::inplace_stop_source _stop_source{};
#ifdef _MSC_VER
	[[msvc::no_unique_address]]
#else
	[[no_unique_address]]
#endif
	on_stop_type _on_stop{};

	auto _arm() noexcept -> void
	{
		if constexpr (_uses_stop_callback)
		{
			_on_stop.emplace(::stdexec::get_stop_token(::stdexec::get_env(_receiver)), forward_stop{ this });
		}
	}

	auto _fail(::std::exception_ptr error) noexcept -> void
	{
		// error 覆盖 stopped：exchange 到 error 后，先前若是 value/stopped 都收下这个错误。
		switch (_kind.exchange(completion_kind::error, ::std::memory_order_acq_rel))
		{
		case completion_kind::value:
			_stop_source.request_stop();
			[[fallthrough]];
		case completion_kind::stopped:
			_error = ::std::move(error);
			break;
		case completion_kind::error:
			break;
		}
	}

	auto _stopped() noexcept -> void
	{
		auto expected = completion_kind::value;
		if (_kind.compare_exchange_strong(expected, completion_kind::stopped, ::std::memory_order_acq_rel))
			_stop_source.request_stop();
	}

	auto _arrive() noexcept -> void
	{
		if (_pending.fetch_sub(1, ::std::memory_order_acq_rel) != 1)
			return;

		if constexpr (_uses_stop_callback)
		{
			_on_stop.reset();
		}

		using enum completion_kind;
		switch (_kind.load(::std::memory_order_acquire))
		{
		case value:
			::stdexec::set_value(::std::move(_receiver));
			return;
		case error:
			::stdexec::set_error(::std::move(_receiver), ::std::move(_error));
			return;
		case stopped:
			::stdexec::set_stopped(::std::move(_receiver));
			return;
		}
	}
};

template <class ReceiverType>
struct when_all_range_child_receiver
{
	using receiver_concept = ::stdexec::receiver_t;

	when_all_range_state<ReceiverType>* _state;

	constexpr auto set_value() const noexcept -> void { _state->_arrive(); }

	constexpr auto set_error(::std::exception_ptr error) const noexcept -> void
	{
		_state->_fail(::std::move(error));
		_state->_arrive();
	}

	constexpr auto set_stopped() const noexcept -> void
	{
		_state->_stopped();
		_state->_arrive();
	}

	// 只覆盖 stop token：其余 query 仍走外层 env。内部 source 才能在兄弟失败时 request_stop。
	[[nodiscard]] constexpr auto get_env() const noexcept
	{
		return ::stdexec::env{
			::stdexec::prop{::stdexec::get_stop_token, _state->_stop_source.get_token()},
			::stdexec::get_env(_state->_receiver),
		};
	}
};

template <::std::ranges::sized_range ChildrenRange>
	requires ::std::move_constructible<ChildrenRange>&& ::stdexec::sender<::std::ranges::range_value_t<ChildrenRange>>
struct when_all_range_sender
{
	using children_range_type = ChildrenRange;
	using sender_concept = ::stdexec::sender_t;
	using completion_signatures = ::stdexec::completion_signatures<
		::stdexec::set_value_t(),
		::stdexec::set_error_t(::std::exception_ptr),
		::stdexec::set_stopped_t()>;

	using _child_sender_type = ::std::ranges::range_value_t<children_range_type>;

	children_range_type _children;

	template <class ReceiverType>
	struct operation_type : when_all_range_state<ReceiverType>
	{
		using receiver_type = ReceiverType;
		using operation_state_concept = ::stdexec::operation_state_t;
		using state_type = when_all_range_state<ReceiverType>;
		using child_receiver_type = when_all_range_child_receiver<ReceiverType>;
		using child_operation_type = ::stdexec::connect_result_t<_child_sender_type, child_receiver_type>;
		using slot_type = manual_lifetime<child_operation_type>;

		// 孩子连的是 `child_receiver_type`，它的 env 改过 stop token —— 要查的是这个 env，
		// 不是外层 receiver 的。
		static_assert(::stdexec::sender_in<_child_sender_type, ::stdexec::env_of_t<child_receiver_type>>);

		::std::unique_ptr<slot_type[]> _operations{};

		constexpr operation_type(children_range_type sources, receiver_type receiver)
			: state_type{
				._receiver = ::std::move(receiver),
				._count = ::std::ranges::size(sources),
				// 第 k 次 arrive 必然在第 k 次 start 之后，所以计数归零必然在全部 start 之后；
				// 起始不需要额外那枚"发起者计数"。
				._pending = ::std::ranges::size(sources) }
		{
			if (this->_count == 0)
				return; // 空 range 不分配

			_operations = ::std::make_unique<slot_type[]>(this->_count);

			// 槽里不记 `_engaged`，所以中途抛异常得就地把已经连上的那些拆掉。
			// 计数是个局部量：构造函数抛异常时析构函数不会被调用，没必要留成成员。
			auto constructed = ::std::size_t{ 0 };
			try
			{
				for (auto&& [source, slot] :
					::std::views::zip(::std::move(sources), ::std::span{ _operations.get(), this->_count }))
				{
					slot.construct_from([&] {
						return ::stdexec::connect(::std::move(source), child_receiver_type{ this });
						});
					++constructed;
				}
			}
			catch (...)
			{
				while (constructed != 0)
				{
					_operations[--constructed].destroy();
				}
				throw;
			}
		}

		// op-state 是地址敏感的：声明了移动构造就把拷贝一并删掉了，不用再写拷贝那两条。
		operation_type(operation_type&&) = delete;
		auto operator=(operation_type&&) -> operation_type & = delete;

		// 构造成功就意味着 `_count` 个槽全连上了；构造失败的话这个析构函数压根不会被调用。
		constexpr ~operation_type() noexcept
		{
			for (auto index = this->_count; index != 0; --index)
			{
				_operations[index - 1].destroy();
			}
		}

		auto start() & noexcept -> void
		{
			if (this->_count == 0)
			{
				if (::stdexec::get_stop_token(::stdexec::get_env(this->_receiver)).stop_requested())
					::stdexec::set_stopped(::std::move(this->_receiver));
				else
					::stdexec::set_value(::std::move(this->_receiver));
				return;
			}
			this->_arm();
			for (auto&& slot : ::std::span{ _operations.get(), this->_count })
			{
				::stdexec::start(slot.get());
			}
		}
	};

	// 单发射：孩子只可移动，连接即消费。`let_value` 以右值连接它返回的 sender
	// （__let.hpp 里 `__nothrow_connectable<__sndr2_t, __rcvr2_t>` 用的是非引用类型），
	// `run_frame` 也是就地构造后立刻连，两处都够用。
	[[nodiscard]] auto connect(::stdexec::receiver auto receiver) && -> operation_type<decltype(receiver)>
	{
		return operation_type<decltype(receiver)>{::std::move(_children), ::std::move(receiver)};
	}
};


template<class T>
when_all_range_sender(T&&) -> when_all_range_sender<::std::views::all_t<T>>;

struct when_all_range_t
{
	[[nodiscard]] constexpr auto operator()(::std::ranges::viewable_range auto&& children) const
		requires ::stdexec::sender<::std::ranges::range_value_t<decltype(children)>>
	{
		return when_all_range_sender(::std::forward<decltype(children)>(children));
	}
};

inline constexpr when_all_range_t when_all_range{};

NAGISA_BUILD_LIB_DETAIL_END