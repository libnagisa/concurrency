# 02 · 快速上手

阅读本章后你将能：

- 写出第一个返回值的协程，并以两种方式跑起来：作为协程被 `co_await`、作为 stdexec sender 被 `sync_wait`。
- 用 `spawn` 把协程 detach 启动到一个线程池上。
- 理解 `simple_task` 的 6 个模板参数各自的意义和默认值。

如果你完全不熟 C++20 协程，先看[第 1 章](./01_concepts.md)。

---

## 1. 准备

依赖：

- 编译器：MSVC ≥ 19.29 / Clang ≥ 17 / GCC ≥ 13（任一即可，需开 C++20）
- [`stdexec`](https://github.com/NVIDIA/stdexec)（NVIDIA 实现的 P2300 senders/receivers）

本库是纯头库，把 `include/` 加入搜索路径即可，无需链接。

约定本章所有代码都以 `namespace nc = ::nagisa::concurrency;` 起手。

---

## 2. Hello, simple_task

```cpp
#include <nagisa/concurrency/concurrency.h>
#include <stdexec/execution.hpp>
#include <iostream>

namespace nc = nagisa::concurrency;

nc::simple_task<int> answer() {
    co_return 42;
}

int main() {
    auto [n] = stdexec::sync_wait(answer()).value();
    std::cout << "answer = " << n << '\n';
}
```

`simple_task<T>` 是一个**返回 `T` 的协程类型**。本身既能被 `co_await`，又能作为 stdexec sender 被 `sync_wait` / `start_on` 等组合。

这段代码的执行流：

1. `answer()` 调用 → 编译器构造 `simple_promise<int, ...>` → 调 `get_return_object()` → 得到 `simple_task<int>`。
2. 因为 `simple_promise` 默认 lazy（`initial_suspend` 是 `suspend_always`），协程**没**真的开始跑，只是分配了 frame，停在了第一条语句之前。
3. `sync_wait(answer())` 把 task 当 sender 用，stdexec 内部对它执行 connect/start，这才驱动 task 跑完，并把 `co_return` 的 42 拿出来包装成 `std::tuple<int>`。
4. `auto [n] = ...` 结构化绑定取出。

---

## 3. 协程嵌套：`co_await` 另一个 task

```cpp
nc::simple_task<int> hi_again() {
    std::cout << "Hello world! Have an int.\n";
    co_return 13;
}

nc::simple_task<int> add_42(int arg) {
    co_return arg + 42;
}

int main() {
    exec::static_thread_pool pool{ 8 };
    stdexec::scheduler auto sch = pool.get_scheduler();

    stdexec::sender auto pipe =
          stdexec::schedule(sch)
        | stdexec::let_value(hi_again)     // 在 pool 上跑 hi_again
        | stdexec::let_value(add_42);      // 把 13 喂给 add_42

    auto [n] = stdexec::sync_wait(std::move(pipe)).value();
    std::cout << "Result: " << n << '\n';   // 55
}
```

完整版见 [example/hello_world.cpp](../example/hello_world.cpp)。

这里 `hi_again` 和 `add_42` 都是 task；它们被 `let_value` 当 sender-from-function 使用——拿到上游的值后构造新 task 并接入流水线。这种 task ↔ sender 的双向互通是本库与 stdexec 集成的核心，详见[第 3 章](./03_with_stdexec.md)。

---

## 4. 在协程里 `co_await` sender

`simple_promise` 装了 [`with_await_transform`](./05_components.md#11-让-sender-可-co_await)，所以协程内部可以直接 `co_await` 任何 sender：

```cpp
nc::simple_task<void> demo() {
    // 跳到当前 scheduler 上恢复执行
    co_await stdexec::schedule(co_await stdexec::get_scheduler());

    // 拿 stop_token
    auto token = co_await stdexec::get_stop_token();
    std::cout << std::boolalpha << token.stop_requested() << '\n';

    // 等多个 sender 都完成
    co_await stdexec::when_all(
        stdexec::just(42),
        stdexec::get_scheduler()
    );
}
```

`co_await stdexec::get_scheduler()` / `co_await stdexec::get_stop_token()` 这两个查询能拿到值，是因为 `simple_promise` 通过 `get_env()` 把 scheduler 和 stop_token 都暴露给了 stdexec。

---

## 5. `spawn`：fire-and-forget

如果你想"启动一个协程在后台跑，我不关心它什么时候完"，用 `spawn`：

```cpp
#include <thread>
#include <nagisa/concurrency/concurrency.h>
#include <exec/static_thread_pool.hpp>

namespace nc = nagisa::concurrency;

nc::simple_task<void> work(int id) {
    std::cout << "work " << id << " on thread " << std::this_thread::get_id() << '\n';
    co_return;
}

int main() {
    exec::static_thread_pool pool{ 4 };
    nc::spawn(pool.get_scheduler(), work(1));
    nc::spawn(pool.get_scheduler(), work(2));
    nc::spawn(pool.get_scheduler(), work(3));
    // 给后台跑的时间
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}
```

`spawn` 的语义：

- 把 `work(...)` 包成一个内部的 driver 协程，driver 跳到 `scheduler` 上执行后 `co_await` 你的 task。
- driver 的 promise 用了 [`exit_then_destroy`](./05_components.md#8-帧回收策略)——跑完自己 destroy 自己，**不需要你管销毁**。
- 返回一个已 release 的裸 handle，你可以无视它。

⚠️ **`spawn` 默认吞掉异常**。task 抛了什么，没人接，frame 析构时就丢了。如果你需要观测失败，要么不要用 `spawn`，要么写一个外层 task 自己 catch。

参数 `intro_type::eager`（默认）意味着 driver 协程在 `spawn(...)` 调用过程中就开始跑（一直跑到第一个真正挂起的 `co_await`）；`intro_type::lazy` 则把 driver 也放到队列里等 scheduler 自己来取。

---

## 6. `simple_task` 的 6 个模板参数

```cpp
template<
    class Value,                                       // ① 返回值类型
    bool Throw = true,                                 // ② 异常透传开关
    class Scheduler = stdexec::inline_scheduler,       // ③ scheduler 类型
    intro_type Intro = intro_type::lazy,               // ④ 启动模式
    class StopToken = stdexec::inplace_stop_token,     // ⑤ stop_token 类型
    class Handle = std::coroutine_handle<>             // ⑥ continuation 句柄类型
>
using simple_task = basic_task<simple_promise<...>, simple_awaitable>;
```

| 参数 | 含义 | 默认 |
|---|---|---|
| `Value` | `co_return` 的值类型；`void` 表示无返回值 | 必填 |
| `Throw` | `true`：捕获异常并在 awaiter 处 rethrow。`false`：`unhandled_exception()` 调 `std::abort`，要求协程体 `noexcept` | `true` |
| `Scheduler` | promise 携带的 scheduler 类型；通常用默认或者 `optional<any_scheduler>` 做类型擦除 | `inline_scheduler` |
| `Intro` | `lazy`：构造后等待 resume；`eager`：构造时就跑到第一个挂起点 | `lazy` |
| `StopToken` | promise 携带的 stop_token 类型 | `inplace_stop_token` |
| `Handle` | continuation handle 的类型；通常不用改 | `coroutine_handle<>` |

常用变体：

```cpp
using my_task         = nc::simple_task<void>;                    // void 返回
using throwless_task  = nc::simple_task<void, false>;             // 不抛异常（必须 noexcept 协程体）
using eager_task      = nc::simple_task<int, true, /*Sched*/,
                                        nc::intro_type::eager>;   // 急切启动
```

---

## 7. 三种执行方式对比

下面是同一个 task 的三种跑法（出自 [example/core.cpp](../example/core.cpp)）：

```cpp
task f1(int i) noexcept { /* ... */ }

// (A) 手动 resume + 让 task 自动销毁
{
    auto t = f1(5);
    t.handle().resume();
    // 作用域结束 → ~task() → destroy
}

// (B) 手动 resume + 手动 destroy
{
    auto t = f1(5);
    t.handle().resume();
    auto h = t.release();   // 把 handle 拿出来
    h.destroy();            // 自行销毁
}

// (C) detach 启动
{
    nc::spawn(stdexec::inline_scheduler{}, f1(5));
    // 不用管
}
```

什么时候用哪种？

- **(A)**：测试 / 同步场景。你知道协程会跑完。
- **(B)**：需要把 handle 交给别人。一旦 `release()` 了，你**必须**保证它最终被 destroy（直接调用，或交给一个会 destroy 它的机制）。
- **(C)**：标准 detach 启动。最省心，但失败信息会丢。

更多生命周期细节在[第 4 章](./04_task_anatomy.md)。

---

## 8. 下一步

你已经能跑 task 了。接下来按需求选：

| 想做的事 | 看哪 |
|---|---|
| 把 task 和 stdexec 算法（when_all / let_value / on / starts_on …）混用 | [第 3 章 · 与 stdexec 集成](./03_with_stdexec.md) |
| 弄清楚 task 的所有权、为什么 `co_await std::move(t)` | [第 4 章 · task 的解剖](./04_task_anatomy.md) |
| 自定义 task 类型（更小、更快、不要某些功能） | [第 5 章 · 组件库速查](./05_components.md) → [第 7 章 · Cookbook](./07_custom_task.md) |
| 看懂"组件组合"的机制 | [第 6 章 · combiner](./06_combiner.md) |
