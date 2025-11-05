#include <iostream>
#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>
#include <nagisa/concurrency/concurrency.h>

namespace nc = ::nagisa::concurrency;

::nc::simple_task<int> hi_again()
{
	::std::cout << "Hello world! Have an int.\n";
	co_return 13;
}
::nc::simple_task<int> add_42(int arg)
{
	co_return arg + 42;
}
::nc::simple_task<int> from_loop()
{
	::std::cout << "from run_loop\n";
	co_return 42;
}
::nc::simple_task<void> all()
{
	co_await ::stdexec::when_all(::stdexec::just(42), ::stdexec::get_scheduler(), ::stdexec::get_stop_token());
}

auto main() -> int {
	::exec::numa_policy numa{ exec::no_numa_policy{} };
	::exec::static_thread_pool ctx{ 8 };
	::stdexec::scheduler auto sch = ctx.get_scheduler();

	::stdexec::sender auto begin = ::stdexec::schedule(sch);
	::stdexec::sender auto hi_again = begin | ::stdexec::let_value(::hi_again);
	::stdexec::sender auto add_42 = hi_again | ::stdexec::let_value(::add_42);
	auto [i] = ::stdexec::sync_wait(::std::move(add_42)).value();
	::std::cout << "Result: " << i << ::std::endl;

	// Sync_wait provides a run_loop scheduler
	std::tuple<::stdexec::run_loop::__scheduler> t = ::stdexec::sync_wait(::stdexec::get_scheduler()).value();
	(void)t;

	auto y = ::stdexec::let_value(::stdexec::get_scheduler(), [](auto sched) {
		return ::stdexec::starts_on(sched, ::stdexec::just() | ::stdexec::let_value(::from_loop));
		});
	::stdexec::sync_wait(std::move(y));

	::stdexec::sync_wait(::all());
}