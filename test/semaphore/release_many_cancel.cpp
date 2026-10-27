#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

#include <array>

// 批量释放把多个等待者一次摘下来、锁外再逐个唤醒。被摘下来但还没轮到的等待者，
// 此时收到取消：它已经不在队列里，_cancel 必须返回 false（由唤醒方完成它），
// 不能 set_stopped，更不能完成两次。
//
// 确定性构造：让第一个等待者的完成函数里去取消第二个等待者的 stop source。
// 此时第二个节点已被摘下（在本地链表里），但还没被调用 _complete。
namespace
{
	struct canceller_receiver
	{
		using receiver_concept = ::stdexec::receiver_t;
		completion* done;
		::stdexec::inplace_stop_source* stop_other;   // 完成时去取消另一个
		::stdexec::inplace_stop_token token;
		void set_value() noexcept
		{
			*done = completion::value;
			if (stop_other != nullptr)
				stop_other->request_stop();
		}
		void set_stopped() noexcept { *done = completion::stopped; }
		void set_error(::std::exception_ptr) noexcept { ::std::terminate(); }
		[[nodiscard]] auto get_env() const noexcept
		{
			return ::stdexec::prop{::stdexec::get_stop_token, token};
		}
	};
}

TEST_CASE("cancelling a waiter that release(n) already detached does not complete it twice")
{
	nc::counting_semaphore<> sem{0};
	::stdexec::inplace_stop_source first_source{};
	::stdexec::inplace_stop_source second_source{};
	completion first_done = completion::pending;
	completion second_done = completion::pending;

	auto op1 = ::stdexec::connect(sem.acquire(),
		canceller_receiver{&first_done, &second_source, first_source.get_token()});
	auto op2 = ::stdexec::connect(sem.acquire(),
		canceller_receiver{&second_done, nullptr, second_source.get_token()});
	::stdexec::start(op1);
	::stdexec::start(op2);

	sem.release(2);

	// 第二个等待者在被唤醒前被取消：它已经被摘下，必须按"成功获得许可"完成一次，
	// 而不是 stopped，也不是两次。
	CHECK(first_done == completion::value);
	CHECK(second_done == completion::value);
	CHECK_FALSE(sem.try_acquire());
}
