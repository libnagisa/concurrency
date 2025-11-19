#include "./erased_task.h"

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