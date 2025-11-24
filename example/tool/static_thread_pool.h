#pragma once

#include <coroutine>
#include <span>
#include <thread>
#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>

namespace details
{
    static ::std::span<::std::thread> steal_static_thread_pool_threads(::exec::_pool_::static_thread_pool_& pool) noexcept;
    static ::std::span<::std::thread const> steal_static_thread_pool_threads(::exec::_pool_::static_thread_pool_ const& pool) noexcept;

    template<auto P>
    struct thief
    {
        friend ::std::span<::std::thread> steal_static_thread_pool_threads(::exec::_pool_::static_thread_pool_& pool) noexcept
        {
            return pool.*P;
        }
        friend ::std::span<::std::thread const> steal_static_thread_pool_threads(::exec::_pool_::static_thread_pool_ const& pool) noexcept
        {
            return pool.*P;
        }
    };
    template struct thief<&::exec::_pool_::static_thread_pool_::threads_>;

    struct static_thread_pool : private ::exec::_pool_::static_thread_pool_
    {
#if STDEXEC_HAS_STD_RANGES()
        friend struct ::exec::_pool_::schedule_all_t;
#endif
        using task_base = ::exec::_pool_::task_base;

        static_thread_pool() = default;

        static_thread_pool(
            std::uint32_t threadCount,
            ::exec::bwos_params params = {},
            ::exec::numa_policy numa = ::exec::get_numa_policy())
            : ::exec::_pool_::static_thread_pool_(threadCount, params, std::move(numa)) {
        }

        auto threads() noexcept
        {
            return details::steal_static_thread_pool_threads(static_cast<::exec::_pool_::static_thread_pool_&>(*this));
        }
        auto threads() const noexcept
        {
            return details::steal_static_thread_pool_threads(static_cast<::exec::_pool_::static_thread_pool_ const&>(*this));
        }

        // struct scheduler;
        using ::exec::_pool_::static_thread_pool_::scheduler;

        // scheduler get_scheduler() noexcept;
        using ::exec::_pool_::static_thread_pool_::get_scheduler;

        // scheduler get_scheduler_on_thread(std::size_t threadIndex) noexcept;
        using ::exec::_pool_::static_thread_pool_::get_scheduler_on_thread;

        // scheduler get_constrained_scheduler(const nodemask& constraints) noexcept;
        using ::exec::_pool_::static_thread_pool_::get_constrained_scheduler;

        // void request_stop() noexcept;
        using ::exec::_pool_::static_thread_pool_::request_stop;

        // std::uint32_t available_parallelism() const;
        using ::exec::_pool_::static_thread_pool_::available_parallelism;

        // bwos_params params() const;
        using ::exec::_pool_::static_thread_pool_::params;
    };
}
using details::static_thread_pool;