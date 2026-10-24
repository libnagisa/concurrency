#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include "support.h"

TEST_CASE("already-stopped acquire on an empty semaphore completes as stopped")
{
	nc::counting_semaphore<4> sem{0};
	::stdexec::inplace_stop_source source{};
	source.request_stop();
	auto done = completion::pending;
	using recv = capture_receiver<::stdexec::inplace_stop_token>;
	auto op = ::stdexec::connect(sem.acquire(), recv{&done, source.get_token()});
	::stdexec::start(op);
	CHECK(done == completion::stopped);
}
