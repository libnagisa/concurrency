#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

// binary_semaphore 的存储是一个 bool：和手写版一样大。
// 一把 std::mutex 加一个 bool 加一个队列头尾，没有给计数器预留的空间。
namespace
{
	struct hand_written
	{
		::std::mutex mutex;
		bool free;
		void* head;
		void* tail;
	};
}
static_assert(sizeof(nc::binary_semaphore) == sizeof(hand_written));
static_assert(sizeof(nc::binary_semaphore_guard) == sizeof(hand_written));

TEST_CASE("binary_semaphore: acquire blocks until the single permit is released")
{
	nc::binary_semaphore sem{0};
	auto done = completion::pending;
	auto op = ::stdexec::connect(sem.acquire(), capture_receiver<>{&done});
	::stdexec::start(op);
	CHECK(done == completion::pending);
	sem.release();
	CHECK(done == completion::value);
	CHECK_FALSE(sem.try_acquire());
	sem.release();
	CHECK(sem.try_acquire());
}

TEST_CASE("binary_semaphore: release hands the permit to the next waiter in FIFO order")
{
	nc::binary_semaphore sem{0};
	auto d1 = completion::pending;
	auto d2 = completion::pending;
	auto op1 = ::stdexec::connect(sem.acquire(), capture_receiver<>{&d1});
	auto op2 = ::stdexec::connect(sem.acquire(), capture_receiver<>{&d2});
	::stdexec::start(op1);
	::stdexec::start(op2);
	sem.release();
	CHECK(d1 == completion::value);
	CHECK(d2 == completion::pending);
	sem.release();
	CHECK(d2 == completion::value);
	CHECK_FALSE(sem.try_acquire());
}
