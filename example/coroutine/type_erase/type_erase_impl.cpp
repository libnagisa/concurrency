#include <ranges>
#include <iostream>

#include "../tool/erased_task.h"

erased_task interface()
{
	// auto s = co_await ::stdexec::get_scheduler();
	// co_await ::stdexec::schedule(s);
	::std::cout << "hello" << ::std::endl;
	co_return;
}