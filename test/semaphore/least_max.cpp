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

// LeastMaxValue 为 1 且没有显式指定 Counter 时，存储是一个标志位：上限恰好 1，
// 和手写的二元信号量一样。显式指定 Counter 则总是计数器。
static_assert(nc::binary_semaphore::max() == 1);
static_assert(nc::basic_semaphore<1, ::std::int8_t>::max() == 127);
static_assert(nc::counting_semaphore<2>::max() > 2);

TEST_CASE("counting_semaphore with a larger LeastMaxValue holds more than that")
{
	nc::counting_semaphore<2> sem{0};
	for (int i = 0; i < 5; ++i)
		sem.release();
	int taken = 0;
	while (sem.try_acquire())
		++taken;
	CHECK(taken == 5);
}

TEST_CASE("an explicit Counter keeps a counter even when LeastMaxValue is 1")
{
	nc::basic_semaphore<1, ::std::int8_t> sem{0};
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
