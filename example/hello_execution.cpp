#include <iostream>
#include <stdexec/execution.hpp>
#include <nagisa/concurrency/concurrency.h>

namespace nc = ::nagisa::concurrency;

template <::stdexec::sender S1, ::stdexec::sender S2>
auto async_answer(S1 s1, S2 s2) -> ::nc::simple_task<int> {
    // Senders are implicitly awaitable (in this coroutine type):
    co_await static_cast<S2&&>(s2);
    co_return co_await static_cast<S1&&>(s1);
}

template <::stdexec::sender S1, ::stdexec::sender S2>
auto async_answer2(S1 s1, S2 s2) -> ::nc::simple_task<std::optional<int>> {
    co_return co_await ::stdexec::stopped_as_optional(async_answer(s1, s2));
}

// tasks have an associated stop token
auto async_stop_token() -> ::nc::simple_task<std::optional<::stdexec::inplace_stop_token>> {
    co_return co_await ::stdexec::stopped_as_optional(::stdexec::get_stop_token());
}

auto main() -> int {
    STDEXEC_TRY{
        // Awaitables are implicitly senders:
        auto [i] = ::stdexec::sync_wait(async_answer2(::stdexec::just(42), ::stdexec::just())).value();
        std::cout << "The answer is " << i.value() << '\n';
    }
        STDEXEC_CATCH(std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
    }
    STDEXEC_CATCH_ALL{
      std::cerr << "unknown error\n";
    }
}