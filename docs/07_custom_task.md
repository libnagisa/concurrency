# 07 · Cookbook：自定义 task 类型

阅读本章后你将能：

- 自己拼一个 task 类型，按需挑选 / 去掉某些功能。
- 看懂三个真实例子是怎么搭出来的：手写版 `simple_task`（[core.cpp](../example/core.cpp)）、轻量并行 task（[benchmark/fibonacci.cpp](../example/benchmark/fibonacci.cpp)）、类型擦除 task（[type_erase/erased_task.h](../example/type_erase/erased_task.h)）。
- 知道什么时候应该自定义而不是用 `simple_task`。

前置：[第 4 章 · task 解剖](./04_task_anatomy.md)、[第 5 章 · 组件库](./05_components.md)、[第 6 章 · combiner](./06_combiner.md)。

---

## 1. 自定义 task 的"骨架"

任何 task 类型至少需要三块：

```cpp
struct my_promise;                       // ① promise

template<class P, class Pa>
using my_awaitable = nc::build_awaitable_t<P, Pa, /* traits */>;  // ② awaitable

using my_task = nc::basic_task<my_promise, my_awaitable>;          // ③ 容器

struct my_promise
    : /* 一堆 promise mixin */
{ };
```

设计步骤：

1. **决定需求**：是否抛异常？是否带 scheduler？是否带 stop_token？是否要返回值？lazy 还是 eager？谁负责销毁？
2. **挑 promise mixin**（[§5.1](./05_components.md#1-命名空间速览)）。
3. **挑配对的 awaitable_trait**——通常和 mixin 成对出现。
4. **写 promise struct**，把所有 mixin 当 base class 列出来，按需补 `get_env()` 之类。
5. **如有需要**，把 mixin 不能覆盖的部分手写补上。

---

## 2. 例 A：手写版 `simple_task`（[core.cpp](../example/core.cpp)）

`simple_task` 本身就是用本库的组件拼的。手写版让你看清每个组件的位置：

```cpp
namespace nc = ::nagisa::concurrency;

struct promise;

template<class Promise, class Parent>
using awaitable = nc::build_awaitable_t<Promise, Parent
    , nc::awaitable_traits::ready_if_done                  // 如果子已结束就别挂
    , nc::awaitable_traits::capture_scheduler              // 把父 scheduler 传给子
    , nc::awaitable_traits::capture_inplace_stop_token     // 把父 stop_token 传给子
    , nc::awaitable_traits::this_then_parent               // 注册父为子的 continuation
    , nc::awaitable_traits::run_this                       // 对称转移到子
    , nc::awaitable_traits::release_value                  // resume 时取出返回值
    , nc::awaitable_traits::rethrow_exception              // resume 时重抛异常
    , nc::awaitable_traits::destroy_after_resumed          // resume 完销毁子的 frame
>;

using task = nc::basic_task<promise, awaitable>;

struct promise
    : nc::promises::lazy                                   // 初始挂起
    , nc::promises::exception<true>                        // 捕获异常
    , nc::promises::value<void>                            // 返回 void
    , nc::promises::jump_to_continuation<>                 // final_suspend 跳 continuation
    , nc::promises::return_object_from_handle<promise, task>  // 合成 get_return_object
    , nc::promises::with_scheduler<>                       // 带 scheduler
    , nc::promises::with_stop_token<>                      // 带 stop_token
    , nc::promises::with_await_transform<promise>          // sender 可 co_await
{
    constexpr auto get_env() const noexcept {
        return stdexec::env(
            nc::promises::with_scheduler<>::get_env(),
            nc::promises::with_stop_token<>::get_env()
        );
    }
};

static_assert(nc::awaitable<awaitable<promise, void>>);
static_assert(nc::awaitable<task>);
```

注意 `get_env()` 是必须手写的——两个 mixin 各自暴露自己那一份 env，需要在 promise 里合并起来。

源码逐字：[example/core.cpp:9-37](../example/core.cpp)。

### 用起来

```cpp
constexpr auto check_stop() noexcept { return check_stop_t{}; }

task f1(int i) noexcept {
    std::cout << i << '\n';
    co_await check_stop();
    co_await stdexec::schedule(co_await stdexec::get_scheduler());
    if (!i) co_return;
    co_await f1(i - 1);
}

int main() {
    {
        auto t = f1(5);
        t.handle().resume();            // 手动启动 lazy
    }                                   // 作用域结束 → destroy
    {
        nc::spawn(stdexec::inline_scheduler{}, f1(5));   // 或者 detach
    }
}
```

---

## 3. 例 B：极简并行 task（[benchmark/fibonacci.cpp](../example/benchmark/fibonacci.cpp)）

这例去掉了 scheduler / stop_token / sender 互通，目标是**性能基准**——越精简越好。

```cpp
struct promise;

template<class Promise, class Parent>
using awaitable = nc::build_awaitable_t<Promise, Parent
    , nc::awaitable_traits::ready_false                  // 永远走挂起
    , nc::awaitable_traits::this_then_parent
    , nc::awaitable_traits::run_this
    , nc::awaitable_traits::release_value
    , nc::awaitable_traits::rethrow_exception
    , nc::awaitable_traits::destroy_after_resumed
>;
using task = nc::basic_task<promise, awaitable>;

struct promise
    : nc::promises::lazy
    , nc::promises::exception<false>                     // 直接 abort，不分配 exception_ptr
    , nc::promises::value<long>                          // 返回 long
    , nc::promises::jump_to_continuation<>
    , nc::promises::return_object_from_handle<promise, task>
    , nc::promises::without_stop_token                   // 不要 stop_token
    , nc::promises::with_await_transform<promise>        // 仍然支持 co_await sender
{};
```

省掉了：
- `capture_scheduler` / `with_scheduler` — 因为 `co_await stdexec::starts_on(sched, ...)` 显式指定，不靠 promise 传递
- `capture_inplace_stop_token` / `with_stop_token` — 不需要取消
- `exception<true>` 改成 `<false>` — fib 计算不会抛，省掉 exception_ptr

效果：协程 frame 更小，hot path 更少分支。

源码：[example/benchmark/fibonacci.cpp:52-72](../example/benchmark/fibonacci.cpp)。

---

## 4. 例 C：类型擦除 task（[type_erase/erased_task.h](../example/type_erase/erased_task.h)）

需要"一个 task 类型，能容纳任何 scheduler"——用 `with_scheduler<std::optional<any_scheduler>>`：

```cpp
struct erased_promise;

template<class P, class Pa>
using erased_awaitable = nc::build_awaitable_t<P, Pa
    , nc::awaitable_traits::ready_false
    , nc::awaitable_traits::this_then_parent
    , nc::awaitable_traits::run_this
    , nc::awaitable_traits::rethrow_exception
    , nc::awaitable_traits::destroy_after_resumed
>;

using erased_task = nc::basic_task<erased_promise, erased_awaitable>;

struct erased_promise
    : nc::promises::lazy
    , nc::promises::return_object_from_handle<erased_promise, erased_task>
    , nc::promises::value<void>
    , nc::promises::exception<true>
    , nc::promises::jump_to_continuation<>
    , nc::promises::without_stop_token
    , nc::promises::with_await_transform<erased_promise>
    , nc::promises::with_scheduler<std::optional<nc::any_scheduler>>   // ← 关键
{};
```

`any_scheduler` 用 `unique_ptr` 内部存任意 scheduler，对外暴露 `stdexec::scheduler` 接口（详细看头文件 [`any_scheduler.h`](../include/nagisa/concurrency/any_scheduler.h)，对应的完整文档章节标 🚧 待写）。

`optional<any_scheduler>` 让 promise 可以**先构造**、**后填**——构造时还不知道 scheduler，第一次被 await 时由 `capture_scheduler` 注入；或者用户手动 `nc::set_scheduler(task.handle().promise(), my_sched)`。

源码：[example/type_erase/erased_task.h](../example/type_erase/erased_task.h)。

---

## 5. 自定义场景速查

| 你想 | 怎么改 |
|---|---|
| 不要异常机制（更小、更快） | `exception<false>` + 协程体 `noexcept`，并去掉 `rethrow_exception` |
| 不要 scheduler | 去掉 `with_scheduler` 和 `capture_scheduler` |
| 不要 stop_token | 把 `with_stop_token<...>` 换成 `without_stop_token`，去掉 `capture_inplace_stop_token` |
| 不能 `co_await` sender | 去掉 `with_await_transform`（很少这么做） |
| eager（立即开始） | `lazy` 换 `eager`（或 `simple_task` 的 `Intro = eager`） |
| detach 自销毁（无 awaiter） | 用 `exit_then_destroy`，去掉 `jump_to_continuation` 和 `this_then_parent` |
| 返回值类型不可移动 | 改 `value<T>` 的存储方式，或用引用包一层 |
| 让 scheduler 类型擦除 | `with_scheduler<std::optional<any_scheduler>>` |
| 多个 task 共享 frame（generator-like） | 不要 `destroy_after_resumed` + 不要 `exit_then_destroy`，自己管生命周期 |

---

## 6. 写一个自己的 trait 组件

需求："每次 await 时把当前线程 id 打印出来"——一次性、不想拿到 `simple_task` 里影响所有协程，所以包成 trait：

```cpp
template<class Promise, class ParentPromise>
struct log_thread_on_suspend {
    using handle_type = std::coroutine_handle<Promise>;

    static void await_suspend(handle_type self,
                              std::coroutine_handle<ParentPromise>) noexcept
    {
        std::cout << "[suspend] " << self.address()
                  << " on tid " << std::this_thread::get_id() << '\n';
    }
};
```

挂到组合里：

```cpp
template<class P, class Pa>
using debug_awaitable = nc::build_awaitable_t<P, Pa
    , nc::awaitable_traits::ready_if_done
    , log_thread_on_suspend                          // ← 新加的
    , nc::awaitable_traits::capture_scheduler
    , nc::awaitable_traits::this_then_parent
    , nc::awaitable_traits::run_this
    , nc::awaitable_traits::release_value
    , nc::awaitable_traits::rethrow_exception
    , nc::awaitable_traits::destroy_after_resumed
>;
```

因为 `log_thread_on_suspend::await_suspend` 返回 `void`，combiner 会把它作为副作用叠到其他几个 `await_suspend` 之前——见[第 6 章 §4](./06_combiner.md#4-同名-hook-怎么合并)。

需要 per-instance 状态时，trait 多加一个 `create_context`，combiner 自动把它打包到 awaiter 的 `_context` tuple 里——参考库内 `capture_stop_token_t` 的 indirect 特化（[stop_token.h:103-135](../include/nagisa/concurrency/_coroutine/component/stop_token.h)）。

---

## 7. 写一个自己的 promise mixin

需求："给每个协程加一个全局 ID"。

```cpp
namespace my_promises {
    struct with_id {
        std::size_t id = ++_next();

        std::size_t get_id() const noexcept { return id; }
    private:
        static std::atomic<std::size_t>& _next() {
            static std::atomic<std::size_t> v{0};
            return v;
        }
    };
}

// 拼到 promise 里
struct my_promise
    : nc::promises::lazy
    , nc::promises::value<void>
    , nc::promises::exception<true>
    , nc::promises::jump_to_continuation<>
    , nc::promises::return_object_from_handle<my_promise, my_task>
    , nc::promises::with_await_transform<my_promise>
    , my_promises::with_id                       // ← 新加的
{};
```

如果你的 mixin 要参与 `get_env()`（比如让某种查询 CPO 能查到 ID），写一个 `env_type` 并在 `get_env()` 里 `stdexec::env(...)` 合并，参考 `with_scheduler` 的实现。

---

## 8. 检查清单

写完 promise + awaitable + task 后，建议加这两个静态断言：

```cpp
static_assert(nc::awaitable<my_awaitable<my_promise, void>>);
static_assert(nc::awaitable<my_task>);
```

如果你的 promise 也要充当 stdexec 的 receiver / 提供 env，可以加：

```cpp
static_assert(stdexec::sender<my_task>);
```

这些断言会在编译期告诉你"差了哪一对组件"。

---

至此，上手指南（00-07）完结。剩下的章节（08 自定义 scheduler、09 stop_token 深入、10 常见陷阱）待补——见 [README](./README.md)。
