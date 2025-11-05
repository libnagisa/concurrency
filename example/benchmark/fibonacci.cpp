#include <cstdlib>
#include <iostream>
#include <execpools/tbb/tbb_thread_pool.hpp>
#include <exec/static_thread_pool.hpp>
#include <exec/any_sender_of.hpp>
#include <stdexec/execution.hpp>
#include <nagisa/concurrency/concurrency.h>

namespace nc = ::nagisa::concurrency;

auto serial_fib(long n) -> long { return n < 2 ? n : serial_fib(n - 1) + serial_fib(n - 2); }

template <class... Ts>
using any_sender_of = ::exec::any_receiver_ref<::stdexec::completion_signatures<Ts...>>::template any_sender<>;
using fib_sender = any_sender_of<stdexec::set_value_t(long)>;
template <typename Scheduler>
struct fib_s {
    using sender_concept = stdexec::sender_t;
    using completion_signatures = stdexec::completion_signatures<stdexec::set_value_t(long)>;
    long cutoff;
    long n;
    Scheduler sched;
    template <class Receiver>
    struct operation {
        Receiver rcvr_;
        long cutoff;
        long n;
        Scheduler sched;
        void start() & noexcept {
            if (n < cutoff) {
                stdexec::set_value(static_cast<Receiver&&>(rcvr_), serial_fib(n));
            }
            else {
                auto mkchild = [&](long n) { return stdexec::starts_on(sched, fib_sender(fib_s{ cutoff, n, sched })); };
                stdexec::start_detached(
                    stdexec::when_all(mkchild(n - 1), mkchild(n - 2))
                    | stdexec::then([rcvr = static_cast<Receiver&&>(rcvr_)](long a, long b) mutable {stdexec::set_value(static_cast<Receiver&&>(rcvr), a + b); }));
            }
        }
    };
    template <stdexec::receiver_of<completion_signatures> Receiver>
    friend auto tag_invoke(stdexec::connect_t, fib_s self, Receiver rcvr) -> operation<Receiver> {
        return { static_cast<Receiver&&>(rcvr), self.cutoff, self.n, self.sched };
    }
};
template <class Scheduler>
fib_s(long cutoff, long n, Scheduler sched) -> fib_s<Scheduler>;


struct promise;

template<class Promise, class Parent>
using awaitable = ::nc::build_awaitable_t<Promise, Parent
    , ::nc::awaitable_traits::ready_false
    , ::nc::awaitable_traits::this_then_parent
    , ::nc::awaitable_traits::run_this
    , ::nc::awaitable_traits::release_value
    , ::nc::awaitable_traits::rethrow_exception
    , ::nc::awaitable_traits::destroy_after_resumed
>;
using task = ::nc::basic_task<promise, awaitable>;
struct promise
    : ::nc::promises::lazy
    , ::nc::promises::exception<false>
    , ::nc::promises::value<long>
    , ::nc::promises::jump_to_continuation<>
    , ::nc::promises::return_object_from_handle<promise, task>
    , ::nc::promises::without_stop_token
    , ::nc::promises::use_as_awaitable<promise>
{};

task fib_coro(auto sche, long n, long cutoff) noexcept
{
    if (n < cutoff)
        co_return ::serial_fib(n);

    auto [a, b] = co_await ::stdexec::when_all(
        ::stdexec::starts_on(sche, ::fib_coro(sche, n - 1, cutoff))
        , ::stdexec::starts_on(sche, ::fib_coro(sche, n - 2, cutoff))
    );
    co_return a + b;
}

template <typename duration, typename F>
auto measure(F&& f) {
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    f();
    return std::chrono::duration_cast<duration>(std::chrono::steady_clock::now() - start).count();
}

auto measure(::std::size_t nruns, auto warmup, auto& pool, auto&& f)
{
    std::vector<unsigned long> times;
    long result;
    for (unsigned long i = 0; i < nruns; ++i) {
        auto snd = std::visit([&](auto&& pool) { return f(pool.get_scheduler()); }, pool);

        auto time = ::measure<std::chrono::milliseconds>(
            [&] { std::tie(result) = stdexec::sync_wait(std::move(snd)).value(); });
        times.push_back(static_cast<unsigned int>(time));
    }

    std::cout << "Avg time: "
        << (std::accumulate(times.begin() + warmup, times.end(), 0u) / (times.size() - warmup))
        << "ms. Result: " << result << std::endl;
}

auto main(int argc, char** argv) -> int {
    if (argc < 5) {
        std::cerr << "Usage: example.benchmark.fibonacci cutoff n nruns {tbb|static}" << std::endl;
        return -1;
    }

    // skip 'warmup' iterations for performance measurements
    static constexpr size_t warmup = 10;

    long cutoff = std::strtol(argv[1], nullptr, 10);
    long n = std::strtol(argv[2], nullptr, 10);
    std::size_t nruns = std::strtoul(argv[3], nullptr, 10);

    if (nruns <= warmup) {
        std::cerr << "nruns should be >= " << warmup << std::endl;
        return -1;
    }

    std::variant<execpools::tbb_thread_pool, exec::static_thread_pool> pool;

    if (argv[4] == std::string_view("tbb")) {
        pool.emplace<execpools::tbb_thread_pool>(static_cast<int>(std::thread::hardware_concurrency()));
    }
    else {
        pool.emplace<exec::static_thread_pool>(
            std::thread::hardware_concurrency(), exec::bwos_params{}, exec::get_numa_policy());
    }

    ::measure(nruns, warmup, pool, [&](auto sche) { return ::fib_coro(sche, n, cutoff); });
    ::measure(nruns, warmup, pool, [&](auto sche) { return fib_sender(fib_s{ cutoff, n, sche }); });
}