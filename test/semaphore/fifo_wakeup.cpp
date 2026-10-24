#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

TEST_CASE("waiting acquires are served FIFO as permits are released")
{
	nc::counting_semaphore<4> sem{0};
	auto d1 = completion::pending;
	auto d2 = completion::pending;
	auto op1 = ::stdexec::connect(sem.acquire(), capture_receiver<>{&d1});
	auto op2 = ::stdexec::connect(sem.acquire(), capture_receiver<>{&d2});
	::stdexec::start(op1);
	::stdexec::start(op2);
	CHECK(d1 == completion::pending);
	CHECK(d2 == completion::pending);

	sem.release();
	CHECK(d1 == completion::value);
	CHECK(d2 == completion::pending);

	sem.release();
	CHECK(d2 == completion::value);
}
