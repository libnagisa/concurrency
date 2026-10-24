#pragma once

/// @file semaphore.h
/// @brief 基于 stdexec 的异步/同步 semaphore。
///
/// - @c basic_semaphore<Max> 是核心实现，@c Max 是编译期上限（对齐
///   std::counting_semaphore 的 LeastMaxValue），@c counting_semaphore<Max>
///   和 @c binary_semaphore（= basic_semaphore<1>）都是它的别名，不是独立实现。
/// - 语义对齐 std::counting_semaphore：acquire/release 手动配对，不自动归还，
///   不携带任何值——可用量只是一个计数器，没有容器、没有 token 搬运。需要
///   "租借一个具体资源对象并在作用域结束时自动归还" 的场景用 lease.h 的
///   lease_pool，不要用 semaphore 硬凑。
/// - 用法：
///     nc::counting_semaphore<4> sem{2};      // 初始 2 个许可，上限 4
///     if (sem.try_acquire()) { ... sem.release(); }
///     co_await sem.acquire();                // 异步排队等待许可
///     sem.release(n);                        // 释放 n 个许可，按 FIFO 唤醒等待者
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

#include "./_semaphore/wait_queue.h"
#include "./_semaphore/basic_semaphore.h"
#include "./_semaphore/counting_semaphore.h"
#include "./_semaphore/binary_semaphore.h"


#include "./environment.h"

NAGISA_BUILD_LIB_BEGIN

using details::wait_queue;
using details::basic_semaphore;
using details::counting_semaphore;
using details::binary_semaphore;

NAGISA_BUILD_LIB_END

#include <nagisa/build_lib/destruct.h>
