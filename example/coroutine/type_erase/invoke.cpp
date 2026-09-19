#include <stdexec/execution.hpp>
#include <iostream>
#include "../tool/erased_task.h"

struct scheduler
{
	constexpr explicit(false) scheduler(::std::size_t const& id) noexcept : _id(::std::addressof(id)) {}
	struct schedule_t : ::std::suspend_never
	{
		auto await_resume() const noexcept
		{
			::std::cout << "scheduler[" << *_id << "] scheduling coroutine on thread " << std::this_thread::get_id() << '\n';
		}
		constexpr auto get_env() const noexcept { return *this; }
		template<class Tag> constexpr auto query(::stdexec::get_completion_scheduler_t<Tag>) const { return scheduler{ *_id }; }
		::std::size_t const* _id;
	};
	constexpr auto schedule() noexcept { return schedule_t{ {}, _id }; }
	constexpr bool operator==(scheduler const&) const noexcept = default;
	::std::size_t const* _id = nullptr;
};
static_assert(::stdexec::scheduler<scheduler>);

erased_task interface();

struct receiver_t : ::stdexec::receiver_t
{
	static void set_error(...) noexcept {}
	static void set_stopped() noexcept {}
	static void set_value(...) noexcept {}
};

void invoke_interface(scheduler s)
{
	auto task = ::interface();
	::nc::set_scheduler(task.handle().promise(), s);
	::stdexec::sync_wait(::stdexec::starts_on(s, ::std::move(task)));
}

::std::size_t a = 0, b = 1, c = 2;

int main()
{
	::invoke_interface(scheduler(a));
	::invoke_interface(scheduler(b));
	::invoke_interface(scheduler(c));

	return 0;
}