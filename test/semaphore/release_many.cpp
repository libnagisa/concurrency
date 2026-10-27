#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

TEST_CASE("release(n) wakes n waiters in FIFO order and keeps the surplus as permits")
{
	nc::counting_semaphore<> sem{0};
	completion done[4] = {};
	using recv = capture_receiver<>;
	auto op0 = ::stdexec::connect(sem.acquire(), recv{&done[0]});
	auto op1 = ::stdexec::connect(sem.acquire(), recv{&done[1]});
	auto op2 = ::stdexec::connect(sem.acquire(), recv{&done[2]});
	::stdexec::start(op0);
	::stdexec::start(op1);
	::stdexec::start(op2);

	sem.release(2);
	CHECK(done[0] == completion::value);
	CHECK(done[1] == completion::value);
	CHECK(done[2] == completion::pending);
	CHECK_FALSE(sem.try_acquire());

	sem.release(3);
	CHECK(done[2] == completion::value);
	CHECK(sem.try_acquire());
	CHECK(sem.try_acquire());
	CHECK_FALSE(sem.try_acquire());
}

TEST_CASE("release(0) and a negative count do nothing")
{
	nc::counting_semaphore<> sem{0};
	sem.release(0);
	CHECK_FALSE(sem.try_acquire());
}

TEST_CASE("a waiter that destroys itself on completion does not break a batch release")
{
	nc::counting_semaphore<> sem{0};
	struct self_destroying
	{
		using receiver_concept = ::stdexec::receiver_t;
		int* hits;
		void set_value() noexcept { ++*hits; }
		void set_stopped() noexcept {}
		void set_error(::std::exception_ptr) noexcept { ::std::terminate(); }
		auto get_env() const noexcept { return ::stdexec::prop{::stdexec::get_stop_token, ::stdexec::never_stop_token{}}; }
	};
	int hits = 0;
	using op_type = ::stdexec::connect_result_t<nc::counting_semaphore<>::acquire_sender, self_destroying>;
	alignas(op_type) unsigned char storage[3][sizeof(op_type)];
	op_type* ops[3];
	for (int i = 0; i < 3; ++i)
	{
		ops[i] = ::new (storage[i]) op_type(::stdexec::connect(sem.acquire(), self_destroying{&hits}));
		::stdexec::start(*ops[i]);
	}
	sem.release(3);
	CHECK(hits == 3);
	for (auto* op : ops)
		op->~op_type();
}
