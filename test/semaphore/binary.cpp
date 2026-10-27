#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

static_assert(nc::binary_semaphore::max() >= 1);

TEST_CASE("binary_semaphore allows only one holder at a time when released once per acquire")
{
	nc::binary_semaphore sem{1};
	CHECK(sem.try_acquire());
	CHECK_FALSE(sem.try_acquire());
	sem.release();
	CHECK(sem.try_acquire());
}
