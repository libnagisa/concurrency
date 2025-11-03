#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <memory>
#include <functional>
#include <coroutine>

#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>
#include <exec/task.hpp>
#include <nagisa/concurrency/concurrency.h>


namespace nc = ::nagisa::concurrency;

::std::atomic_bool cv = false;
::std::mutex mutex;

::nc::simple_task<void> make_coro(auto sche, int id)
{
    co_await ::stdexec::schedule(sche);
    cv.wait(true);
    {
        ::std::unique_lock l(mutex);
        ::std::cout << "[coro] id=" << id << " running on thread " << std::this_thread::get_id() << '\n';
    }
}
::exec::task;
int main() {
    auto num_workers = std::max(1u, std::thread::hardware_concurrency());
    std::cout << "starting static_thread_pool with " << num_workers << " workers\n";

    exec::static_thread_pool pool{ num_workers };
    auto handles
		= ::std::views::iota(0, 7)
		| ::std::views::transform([&](int id) { return make_coro(pool.get_scheduler(), id); })
		| ::std::ranges::to<::std::vector>();
    for (auto&& h : handles)
    {
        h.handle().resume();
    }
    using namespace ::std::chrono_literals;
    std::this_thread::sleep_for(1s);
    cv = true;

   // ::std::jthread t{ [&] { pool.run(); } };

    std::this_thread::sleep_for(1s);

    std::cout << "main: requesting stop\n";
    pool.request_stop();
    return 0;
}