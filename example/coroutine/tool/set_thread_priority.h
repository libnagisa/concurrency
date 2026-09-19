#pragma once

#include <iostream>

#if __has_include(<pthread.h>)
#	define USE_PTHREAD 1
#	include <pthread.h>
#else
#	define USE_PTHREAD 0
#endif

inline void set_thread_priority(auto handle, auto priority)
{
#if USE_PTHREAD
	{
		::sched_param sch_params{};
		sch_params.sched_priority = priority;
		if (::pthread_setschedparam(handle, SCHED_FIFO, &sch_params) != 0) {
			::std::cerr << "Failed to set SCHED_FIFO, need sudo or CAP_SYS_NICE\n";
		}
		else {
			::std::cout << "Thread switched to SCHED_FIFO priority\n";
		}
	}
#endif
}