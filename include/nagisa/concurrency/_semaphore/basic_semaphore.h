#pragma once

#include "./wait_queue.h"
#include "./environment.h"

#include "./macro_push.h"

NAGISA_BUILD_LIB_DETAIL_BEGIN

struct semaphore_waiter
{
	semaphore_waiter* _next = nullptr;
	void (*_complete)(semaphore_waiter*) noexcept = nullptr;
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
					if (!_operation->_semaphore->_unlink(_operation))
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
				if (_semaphore->_acquire_or_enqueue(*this))
				{
					::stdexec::set_value(::std::move(_receiver));
					return;
				}
				if constexpr (_uses_stop_callback)
					_on_stop.emplace(::stdexec::get_stop_token(::stdexec::get_env(_receiver)), on_stop{ this });
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
	[[nodiscard]] bool _acquire_or_enqueue(semaphore_waiter& target) noexcept
	{
		auto guard = ::std::scoped_lock{ _mutex };
		if (_count > 0)
		{
			--_count;
			return true;
		}
		_waiters.push_back(target);
		return false;
	}
	[[nodiscard]] bool _unlink(semaphore_waiter* target) noexcept
	{
		auto guard = ::std::scoped_lock{ _mutex };
		return _waiters.unlink(target);
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
