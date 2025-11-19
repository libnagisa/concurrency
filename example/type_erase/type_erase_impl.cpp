#include <ranges>
#include "./erased_task.h"
#include <iostream>

erased_task interface()
{
	// auto s = co_await ::stdexec::get_scheduler();
	// co_await ::stdexec::schedule(s);
	::std::cout << "hello" << ::std::endl;
	co_return;
}