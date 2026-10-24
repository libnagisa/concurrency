#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

TEST_CASE("already-stopped acquire is cancelled even when a permit is available")
{
	nc::counting_semaphore<4> sem{1};
	::stdexec::inplace_stop_source source{};
	source.request_stop();
	auto done = completion::pending;
	using recv = capture_receiver<::stdexec::inplace_stop_token>;
	auto op = ::stdexec::connect(sem.acquire(), recv{&done, source.get_token()});
	::stdexec::start(op);
	// start() 先订阅取消、再判定是否有可用许可：一旦同步触发了取消，
	// 一律按 cancelled 处理，许可原样留给下一次 acquire/try_acquire。
	CHECK(done == completion::stopped);
	CHECK(sem.try_acquire());
}
