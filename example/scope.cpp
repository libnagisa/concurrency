#include <stdexec/execution.hpp>
#include <exec/async_scope.hpp>
#include <exec/static_thread_pool.hpp>
#include <nagisa/concurrency/concurrency.h>

namespace nc = ::nagisa::concurrency;

#include <cstdio>

///////////////////////////////////////////////////////////////////////////////
// Example code:
using namespace stdexec;
using stdexec::sync_wait;

struct noop_receiver {
    using receiver_concept = receiver_t;

    template <class... _As>
    void set_value(_As&&...) noexcept {
    }

    void set_error(std::exception_ptr) noexcept {
    }

    void set_stopped() noexcept {
    }

    [[nodiscard]]
    auto get_env() const& noexcept {
        return stdexec::prop{ get_stop_token, stdexec::never_stop_token{} };
    }
};

auto main() -> int {
    exec::static_thread_pool ctx{ 1 };
    exec::async_scope scope;

    scheduler auto sch = ctx.get_scheduler(); // 1

    sender auto begin = schedule(sch); // 2

    sender auto printVoid = then(begin, []() noexcept { printf("void\n"); }); // 3

    sender auto printEmpty = then(
        starts_on(sch, scope.on_empty()),
        []() noexcept { // 4
            printf("scope is empty\n");
        });

    printf(
        "\n"
        "spawn void\n"
        "==========\n");

    scope.spawn(printVoid); // 5

    sync_wait(printEmpty);

    printf(
        "\n"
        "spawn void and 42\n"
        "=================\n");

    sender auto fortyTwo = then(begin, []() noexcept { return 42; }); // 6

    scope.spawn(printVoid); // 7

    sender auto fortyTwoFuture = scope.spawn_future(fortyTwo); // 8

    sender auto printFortyTwo = then(
        std::move(fortyTwoFuture),
        [](int fortyTwo) noexcept { // 9
            printf("%d\n", fortyTwo);
        });

    sender auto allDone =
        then(when_all(printEmpty, std::move(printFortyTwo)), [](auto&&...) noexcept {
        printf("\nall done\n");
            }); // 10

    sync_wait(std::move(allDone));

    {
        sender auto nest = scope.nest(begin);
        (void)nest;
    }
    sync_wait(scope.on_empty());

    {
        sender auto nest = scope.nest(begin);
        auto op = connect(std::move(nest), noop_receiver{});
        (void)op;
    }
    sync_wait(scope.on_empty());

    {
        sender auto nest = scope.nest(begin);
        sync_wait(std::move(nest));
    }
    sync_wait(scope.on_empty());
}