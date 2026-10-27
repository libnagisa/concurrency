#pragma once

/// @file semaphore.h
/// @brief 基于 stdexec 的异步/同步 semaphore。
///
/// - @c basic_semaphore<LeastMaxValue, Counter> 是核心实现。@c LeastMaxValue 对齐
///   std::counting_semaphore：保证至少能容纳这么多个许可，不是精确上限；实际
///   上限是 @c max()，即 numeric_limits<Counter>::max()（@c Counter 默认
///   ptrdiff_t）。所以 binary_semaphore 连续 release 两次是合法的，不会失败。
///   @c counting_semaphore<LeastMaxValue> 和 @c binary_semaphore（= basic_semaphore<1>）
///   都是它的别名，不是独立实现。
///   release 超过 max()、构造时 desired 越界、release 传负数是前置条件违反，
///   debug 下断言，release 构建下没有任何检查开销。
/// - 语义对齐 std::counting_semaphore：acquire/release 手动配对，不自动归还，
///   不携带任何值——可用量只是一个计数器，没有容器、没有 token 搬运。需要
///   "租借一个具体资源对象并在作用域结束时自动归还" 的场景用同一个头文件里的
///   @c resource_pool，不要用 semaphore 硬凑。
/// - 用法：
///     nc::counting_semaphore<4> sem{2};      // 初始 2 个许可，上限 4
///     if (sem.try_acquire()) { ... sem.release(); }
///     co_await sem.acquire();                // 异步排队等待许可
///     sem.release(n);                        // 释放 n 个许可，按 FIFO 唤醒等待者
///
/// - 如果你的 acquire/release 总是一一配对、持有期在一个对象生命周期内就能
///   表达清楚，用 @c basic_semaphore_guard<LeastMaxValue>（别名 @c counting_semaphore_guard
///   / @c binary_semaphore_guard）：它内部持有一个 basic_semaphore，acquire()
///   直接复用 basic_semaphore 的 sender（不重新发明 operation state，只用
///   stdexec::then 在完成时包一层），拿到的是一个 permit，析构时自动归还。
///   它不支持 release(n) 批量释放，也不支持"没有对应 acquire 就直接 release"
///   的事件通知式用法——这些用法没有自然的持有者对象可以挂 RAII，需要它们就
///   用 basic_semaphore 本身。
///
/// - @c resource_pool<Token, Container> 是带资源的 semaphore：许可就是容器里的
///   某个 token。acquire()/try_acquire() 得到一个 @c permit，持有 token，析构时
///   自动归还；归还时若有等待者，token 不经过容器，直接交给队首。
///   @c bounded_resource_pool<Token, N> 是容量固定的版本。
///
/// @c acquire() 的完成信号随容器而定：容器的 empty()/back()/pop_back() 都是
/// noexcept 时（vector、deque、bounded_stack）只有 set_value/set_stopped；
/// 任何一个可能抛，才多出 set_error(exception_ptr)。归还时（permit 析构）
/// 容器 push_back 抛异常没有地方可以报告，按 terminate 处理；有等待者时 token
/// 直接交接，不碰容器。
///
/// 实现上 basic_semaphore 和 resource_pool 共用 @c acquire_core（一把锁、一条
/// 等待队列、同一套"订阅取消 -> 加锁仲裁 -> 完成"状态机），只是 Store 不同：
/// 计数器不带值，容器带 token。两者各自实例化，没有为对方付出任何运行期开销。
///
/// 已知限制（爆栈风险）：@c release() 唤醒排队中的等待者时，是在调用
/// @c release() 的那个线程、那个栈帧上，同步、直接调用等待者的完成函数
/// （@c set_value / @c set_stopped）。如果完成之后的延续代码又同步调用
/// @c release()（典型场景：A 释放许可唤醒 B，B 处理完立刻又释放许可唤醒 C……
/// 形成链式反应），这条调用链的栈深度与链长成正比——C++ 标准不保证这里的
/// 调用会被优化成尾调用（不像协程的 @c final_suspend 那样，尾调用是语言标准
/// 强制要求的变换），因此极长的同步唤醒链在理论上可能耗尽栈空间。
/// 这是"同步完成协议 + 非协程 receiver"组合的结构性限制，不是实现缺陷，
/// semaphore 自身无法单方面解决（它不知道、也不该假设下游做了什么）。
/// 如果你的使用场景可能产生很长的链式唤醒，建议用 @c co_await 消费
/// @c acquire()（享受协程 @c resume() 的标准尾调用保证），或者自己在
/// 唤醒链路上插入调度器做显式的异步转移。

#include "./_semaphore/basic_semaphore.h"
#include "./_semaphore/counting_semaphore.h"
#include "./_semaphore/binary_semaphore.h"
#include "./_semaphore/semaphore_guard.h"
#include "./_semaphore/resource_pool.h"


#include "./environment.h"

NAGISA_BUILD_LIB_BEGIN

using details::basic_semaphore;
using details::counting_semaphore;
using details::binary_semaphore;
using details::basic_semaphore_guard;
using details::counting_semaphore_guard;
using details::binary_semaphore_guard;
using details::resource_token;
using details::resource_pool;
using details::bounded_stack;
using details::bounded_resource_pool;

NAGISA_BUILD_LIB_END

#include <nagisa/build_lib/destruct.h>
