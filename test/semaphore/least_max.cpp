#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

#include <cstdint>
#include <limits>

// LeastMaxValue 是下限（对齐 std::counting_semaphore），实际上限是 max()。
static_assert(nc::binary_semaphore::max() >= 1);
static_assert(nc::counting_semaphore<10>::max() >= 10);
static_assert(nc::basic_semaphore<10, ::std::int8_t>::max() == 127);
static_assert(nc::basic_semaphore<255, ::std::uint8_t>::max() == 255);

TEST_CASE("binary_semaphore may hold more than one permit: LeastMaxValue is a lower bound")
{
	nc::binary_semaphore sem{0};
	sem.release();
	sem.release();
	CHECK(sem.try_acquire());
	CHECK(sem.try_acquire());
	CHECK_FALSE(sem.try_acquire());
}

TEST_CASE("a narrow counter holds up to its own max")
{
	nc::basic_semaphore<10, ::std::uint8_t> sem{0};
	sem.release(255);
	int taken = 0;
	while (sem.try_acquire())
		++taken;
	CHECK(taken == 255);
}
