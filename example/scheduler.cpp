#include <iostream>
#include <coroutine>

#include <nagisa/concurrency/concurrency.h>

namespace nc = ::nagisa::concurrency;

struct scheduler
{
	constexpr explicit(false) scheduler(::std::size_t const& id) noexcept
		: _id(::std::addressof(id))
	{}

	struct schedule_t : ::std::suspend_never
	{
		auto await_resume() const noexcept
		{
			::std::cout << "scheduler[" << *_scheduler->_id << "] scheduling coroutine on thread " << std::this_thread::get_id() << '\n';
			return false;
		}

		constexpr auto&& get_env() const noexcept { return *this; }
		template<class Tag>
		constexpr auto query(::stdexec::get_completion_scheduler_t<Tag>) const noexcept { return *_scheduler; }

		scheduler* _scheduler;
	};
	constexpr auto schedule() noexcept { return schedule_t{ {}, this }; }
	constexpr bool operator==(scheduler const&) const noexcept = default;

	::std::size_t const* _id = nullptr;
};
static_assert(::stdexec::scheduler<scheduler>);
static_assert(::stdexec::scheduler<::nc::any_scheduler>);
using optional_task = ::nc::simple_task<void, false, ::std::optional<::nc::any_scheduler>>;

optional_task f1() noexcept
{
	auto scheduler = co_await ::stdexec::get_scheduler();
	co_await ::stdexec::schedule(scheduler);
}

optional_task f2() noexcept
{
	auto scheduler = co_await ::stdexec::get_scheduler();
	co_await ::stdexec::schedule(scheduler);
	co_await ::f1();
}

using task = ::nc::simple_task<void, false, scheduler, ::nc::intro_type::eager>;
task g2(auto&&) noexcept
{
	auto scheduler = co_await ::stdexec::get_scheduler();
	co_await ::stdexec::schedule(scheduler);
	co_await ::f1();
}

::std::size_t a = 1;
int main()
{
	struct
	{
		constexpr auto query(::stdexec::get_scheduler_t) const noexcept { return scheduler(a); }
	}env{};
	{
		auto task = f2();
		::nc::set_scheduler(task.handle().promise(), scheduler(a));
		task.handle()();
	}
	{
		g2(env);
	}

	return 0;
}
