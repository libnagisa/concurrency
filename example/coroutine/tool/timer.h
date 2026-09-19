#pragma once

#include <chrono>
#include <coroutine>
#include <ranges>

#include <stdexec/execution.hpp>
#include <nagisa/concurrency/concurrency.h>
#include <exec/scope.hpp>

template<class TimePoint>
struct timer_task_t
{
	using time_point = TimePoint;
	::std::coroutine_handle<> handle;
	::std::coroutine_handle<> stop;
	time_point due;
	constexpr auto operator<=>(timer_task_t const& other) const noexcept { return due <=> other.due; }
};

template<class Clock = ::std::chrono::steady_clock>
struct timer
{
	// using Clock = ::std::chrono::steady_clock;
	using self_type = timer;
	using clock_type = Clock;
	using time_point_type = typename clock_type::time_point;
	using duration_type = typename clock_type::duration;

	struct task_queue
	{
		using task_type = timer_task_t<time_point_type>;

		void push(task_type const& task) noexcept
		{
			auto lk = ::std::unique_lock(_mutex);
			_tasks.push(task);
			_cv.notify_one();
		}
		::std::optional<task_type> pop(::stdexec::stoppable_token auto const& stop_token) noexcept
		{
			auto lk = ::std::unique_lock(_mutex);
			auto notify = [this] { _cv.notify_all(); };
			auto callback = ::stdexec::stop_callback_for_t<::std::remove_cvref_t<decltype(stop_token)>, decltype(notify)>(stop_token, notify);
			while (true)
			{
				if (_tasks.empty())
					_cv.wait(lk, [this, &stop_token] { return stop_token.stop_requested() || !_tasks.empty(); });
				if (stop_token.stop_requested())
					return ::std::nullopt;
				auto task = _tasks.top();
				if (task.due <= clock_type::now())
				{
					_tasks.pop();
					return task;
				}
				auto status = _cv.wait_until(lk, task.due);
				if (stop_token.stop_requested())
					return ::std::nullopt;
			}
		}

		::std::mutex _mutex{};
		::std::condition_variable _cv{};
		::std::priority_queue<task_type, ::std::vector<task_type>, ::std::greater<>> _tasks{};
	};

	auto run(::stdexec::scheduler auto scheduler) noexcept
	{
		constexpr auto submit_handle = [](::std::coroutine_handle<> handle) noexcept
			{
				return ::stdexec::just(handle) | ::stdexec::then([](::std::coroutine_handle<> handle) { handle.resume(); });
			};
		while (!_scope.get_stop_source().stop_requested())
		{
			auto task = _queue.pop(_scope.get_stop_token());
			if (!task)
				break;
			_scope.spawn(::stdexec::starts_on(scheduler, submit_handle(task->handle)));
		}
		auto lock = ::std::unique_lock(_queue._mutex);
		while (!_queue._tasks.empty())
		{
			auto task = _queue._tasks.top();
			_scope.spawn(::stdexec::starts_on(scheduler, submit_handle(task.stop)));
			_queue._tasks.pop();
		}
	}
	auto&& async_scope() noexcept { return _scope; }

	struct scheduler
	{
		self_type* _self;
		duration_type _default_interval = {};
		struct at : ::std::suspend_always
		{
			constexpr auto&& get_env() const noexcept { return *this; }
			template<class Tag>
			constexpr auto query(::stdexec::get_completion_scheduler_t<Tag>) const noexcept { return scheduler{ _self }; }
			template<class Promise>
			auto await_suspend(::std::coroutine_handle<Promise> parent) const noexcept
			{
				if constexpr (requires { parent.promise().unhandled_stopped(); })
				{
					return _self->_queue.push({ parent, parent.promise().unhandled_stopped(), _due });
				}
				else
				{
					return _self->_queue.push({ parent, ::std::noop_coroutine(), _due });
				}
			}
			self_type* _self;
			time_point_type _due;
		};
		constexpr static auto now() noexcept { return clock_type::now(); }
		constexpr auto schedule() const noexcept { return scheduler::schedule_at(now() + _default_interval); }
		constexpr auto schedule_at(time_point_type tp) const noexcept { return at{ {}, _self, tp }; }
		constexpr bool operator==(const scheduler&) const = default;
	};
	constexpr auto get_scheduler() noexcept { return scheduler{ this }; }
	constexpr auto get_scheduler(duration_type duration) noexcept { return scheduler{ this, duration }; }

	timer() = default;
	~timer()
	{
		
	}

	task_queue _queue{};
	::exec::async_scope _scope{};
};

template<class Clock = ::std::chrono::steady_clock>
struct join_timer : timer<Clock>
{
	join_timer(::stdexec::scheduler auto scheduler)
		: _schedule_thread([this, scheduler] { this->run(::std::move(scheduler)); })
	{}
	~join_timer()
	{
		if (!this->_scope.get_stop_source().stop_requested())
		{
			join();
		}
	}

	void join()
	{
		this->_scope.request_stop();
		::stdexec::sync_wait(this->_scope.on_empty());
		if (_schedule_thread.joinable())
			_schedule_thread.join();
	}

	::std::jthread _schedule_thread;
};