#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

TEST_CASE("try_acquire respects the current count and release restores it")
{
	nc::counting_semaphore<4> sem{2};
	CHECK(sem.try_acquire());
	CHECK(sem.try_acquire());
	CHECK_FALSE(sem.try_acquire());
	sem.release();
	CHECK(sem.try_acquire());
}
