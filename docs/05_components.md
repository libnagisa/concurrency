# 05 · 组件库速查

这一章是**速查表**，列出库里所有可以塞进 task 类型的组件，按"promise 组件"和"awaitable_trait 组件"两条主线组织。每个条目说明：

- 它在协程的哪个 hook 上工作
- 配套通常需要哪个对应组件
- 一句话语义

读完本章你能：

- 在拼自己的 task 类型时知道挑哪些组件
- 看懂源码里 `simple_promise` / `spawn_promise` 这些"现成 task"是怎么搭出来的
- 知道每个 CPO 的作用

需要更深的"为什么"和"怎么自己写组件"看[第 6 章](./06_combiner.md)。

---

## 1. 命名空间速览

| 命名空间 | 内容 |
|---|---|
| `nc::promises` | 所有 promise mixin（控制协程**自己**怎么活） |
| `nc::awaitable_traits` | 所有 awaitable trait（控制协程**被等待**时怎么表现） |
| 顶层（`nc::*`） | CPO（`set_scheduler` / `set_stop_token` / `set_continuation` / `continuation` / `get_unhandled_exception_ptr` / `release_returned_value`），以及工具 awaitable |

---

## 2. 配对总览

下表显示典型的"promise 组件 ↔ awaitable_trait 组件"配对。两边都装上，才能完整实现某个功能。

| 功能 | promise 组件 | 配对的 awaitable_trait | 第几节 |
|---|---|---|---|
| 启动语义（lazy / eager） | `lazy` / `eager` | `ready_if_done` / `ready_true` / `ready_false` | §3 |
| 异常透传 | `exception<true>` / `exception<false>` | `rethrow_exception` | §4 |
| 返回值传递 | `value<T>` / `value<void>` | `release_value` | §5 |
| return_object 合成 | `return_object_from_handle<P, T>` | — | §6 |
| continuation 跳转 | `jump_to_continuation<H>` | `this_then_parent` / `parent_then_this` | §7 |
| 帧回收策略 | `default_exit` / `exit_then_destroy` | `destroy_after_resumed` | §8 |
| scheduler 透传 | `with_scheduler<S>` | `capture_scheduler` | §9 |
| stop_token 透传 | `with_stop_token<T>` / `without_stop_token` | `capture_inplace_stop_token` / `capture_stop_token<S>` | §10 |
| 让 sender 可 co_await | `with_await_transform<Derived>` | — | §11 |
| 父协程跳转语义 | — | `run_this` / `run_parent` / `default_suspend` | §12 |
| 一次性 lambda hook | — | `on_ready` / `on_suspend` / `on_resume` | §13 |

每个组件单独装也合法（只是少了对应那一半的功能），但成对才是常见用法。

---

## 3. 启动语义

源码：[intro.h](../include/nagisa/concurrency/_coroutine/component/intro.h)

| 组件 | 作用 |
|---|---|
| `promises::lazy` | `initial_suspend()` 返回 `suspend_always`——协程创建时不跑，等被 resume |
| `promises::eager` | `initial_suspend()` 返回 `suspend_never`——协程创建即跑到第一个挂起点 |
| `awaitable_traits::ready_false` | `await_ready()` 永远 false——总是走挂起逻辑 |
| `awaitable_traits::ready_true` | `await_ready()` 永远 true——总是同步出结果 |
| `awaitable_traits::ready_if_done` | `await_ready()` 返回 `handle.done()`——如果协程已结束就不挂 |

还有一个 `enum class intro_type { lazy, eager }`，用作 `simple_task` 等模板的参数开关。

**典型搭配**：
- lazy task：`lazy` + `ready_if_done`（多数情况）或 `ready_false`（强制必须 await_suspend 才启动）
- eager task：`eager` + `ready_if_done`（同步完成时短路）

---

## 4. 异常透传

源码：[exception.h](../include/nagisa/concurrency/_coroutine/component/exception.h)

| 组件 | 作用 |
|---|---|
| `promises::exception<true>` | `unhandled_exception()` 捕获到 `std::exception_ptr`；提供 `get_unhandled_exception_ptr()` |
| `promises::exception<false>` | `unhandled_exception()` 调 `std::abort()`——只适合 `noexcept` 协程体 |
| `awaitable_traits::rethrow_exception` | `await_resume` 时若有捕获的异常，rethrow 给父协程 |

**CPO**：`nc::get_unhandled_exception_ptr(promise)`——取已捕获的 exception_ptr，支持成员函数或 `tag_invoke`。

---

## 5. 返回值传递

源码：[value.h](../include/nagisa/concurrency/_coroutine/component/value.h)

| 组件 | 作用 |
|---|---|
| `promises::value<T>` | 提供 `return_value`（move 进内部 `optional`）和 `release_returned_value`（move 出来） |
| `promises::value<void>` | 提供 `return_void` |
| `awaitable_traits::release_value` | `await_resume` 时调 `release_returned_value` 把值移交给 `co_await` 表达式 |

**CPO**：`nc::release_returned_value(promise)`——把 promise 里存的返回值 move 出来。

---

## 6. return_object 合成

源码：[return_object.h](../include/nagisa/concurrency/_coroutine/component/return_object.h)

```cpp
struct my_task;
struct my_promise : promises::return_object_from_handle<my_promise, my_task> { ... };
```

会自动合成：

```cpp
my_task get_return_object() {
    return my_task(std::coroutine_handle<my_promise>::from_promise(*this));
}
```

要求 `my_task` 可由 `coroutine_handle<my_promise>` 构造。CRTP 风格——第一个模板参数是 promise 自己。

C++23 的 "deducing this" 支持单参数版本：

```cpp
struct my_promise : promises::return_object_from_handle<my_task> { ... };
```

但需要编译器开启 `__cpp_explicit_this_parameter`。

---

## 7. continuation 跳转

源码：[continuation.h](../include/nagisa/concurrency/_coroutine/component/continuation.h)、[workflow.h](../include/nagisa/concurrency/_coroutine/component/workflow.h)

这是"父协程 await 子协程 → 子协程结束后回到父协程"这条路径的实现。

| 组件 | 作用 |
|---|---|
| `promises::jump_to_continuation<H>` | `final_suspend` 时对称转移到保存的 continuation handle（默认是 noop） |
| `awaitable_traits::this_then_parent` | `await_suspend` 时调 `set_continuation(child.promise(), parent)`——告诉子协程 "结束后跳回父" |
| `awaitable_traits::parent_then_this` | 把子协程插到父协程现有 continuation **前面**：父结束 → 跳到子 → 子结束 → 跳到原 continuation |

**CPO**：
- `nc::set_continuation(promise, parent)`：设置一个 promise 的 continuation
- `nc::continuation(promise)`：读取 continuation

---

## 8. 帧回收策略

源码：[exit.h](../include/nagisa/concurrency/_coroutine/component/exit.h)

谁负责销毁协程帧？两种典型选择：

| 组件 | 协程跑完时 |
|---|---|
| `promises::default_exit` + `awaitable_traits::destroy_after_resumed` | promise 在 `final_suspend` 时挂起；**awaiter** 在 `await_resume` 后调 `destroy()` |
| `promises::exit_then_destroy` | promise 在 `final_suspend` 时**自己**调 `destroy()`；适合 detached 任务（没有 awaiter 帮你清理） |

第一种是 `simple_task` 用的——task 被 await 时所有权转给 awaiter，awaiter 跑完销毁。第二种是 `spawn_task` 用的——detach 出去没人管，自己活完自己死。

---

## 9. scheduler 透传

源码：[with_scheduler.h](../include/nagisa/concurrency/_coroutine/component/with_scheduler.h)

| 组件 | 作用 |
|---|---|
| `promises::with_scheduler<S>` | promise 携带一个 scheduler `S`；通过 `get_env()` 暴露给 `stdexec::get_scheduler` 查询 |
| `promises::with_scheduler<std::optional<S>>` | 可延迟设置的版本——开始时没 scheduler，可后续填入 |
| `awaitable_traits::capture_scheduler` | `await_suspend` 时从父协程的 env 里捞 scheduler，注入子 promise |

**CPO**：`nc::set_scheduler(promise, sched)`——给 promise 设置 scheduler。

效果是：在协程内部直接写 `co_await stdexec::get_scheduler()` 就能拿到由父协程注入的 scheduler；从而 `co_await stdexec::schedule(sched)` 就能在 scheduler 上恢复执行。

---

## 10. stop_token 透传

源码：[stop_token.h](../include/nagisa/concurrency/_coroutine/component/stop_token.h)

| 组件 | 作用 |
|---|---|
| `promises::without_stop_token` | 不支持取消；`unhandled_stopped` 调 `std::terminate` |
| `promises::with_stop_token<T>` | 携带 stop_token `T`；提供 `get_env()` / `stop_requested()` / `set_stop_token` |
| `awaitable_traits::capture_inplace_stop_token` | 用 `inplace_stop_source` 作适配器透传 stop_token |
| `awaitable_traits::capture_stop_token<Source>` | 同上，但可指定适配器类型 |

**CPO**：`nc::set_stop_token(promise, token)`。

trait 有三种特化路径：
1. 子 promise 类型能直接接受父的 token → 直接复制
2. 类型不匹配但父 token 支持注册 callback → 起一个本地 `Source`，把父 token 的取消通过 callback 转发到本地 source，再把本地 source 的 token 交给子（避免类型擦除开销）
3. 都不行 → 静默 no-op

---

## 11. 让 sender 可 co_await

源码：[with_awaitable.h](../include/nagisa/concurrency/_coroutine/component/with_awaitable.h)

| 组件 | 作用 |
|---|---|
| `promises::with_await_transform<Derived>` | `await_transform` 转发到 `stdexec::as_awaitable`，让任何 sender 都能 `co_await` |

CRTP 风格——`Derived` 是最终 promise。装了它，下面这种代码才合法：

```cpp
co_await stdexec::schedule(my_sched);
co_await stdexec::just(42);
co_await some_sender;
```

---

## 12. 父协程跳转语义

源码：[workflow.h](../include/nagisa/concurrency/_coroutine/component/workflow.h)

`await_suspend` 的返回值决定下一个跑谁，这几个 trait 就是给出常用的几种返回：

| 组件 | `await_suspend` 返回 |
|---|---|
| `awaitable_traits::default_suspend` | `void`——挂起，回到调度器 |
| `awaitable_traits::run_this` | 子 handle——对称转移到子协程 |
| `awaitable_traits::run_parent` | 父 handle——对称转移回父协程（用于已同步完成的子） |

通常和 `this_then_parent` 配套：先 `this_then_parent` 把回路接好，再 `run_this` 跳过去。

---

## 13. 一次性 lambda hook

源码：[custom.h](../include/nagisa/concurrency/_coroutine/component/custom.h)

不想为一次性行为写完整 trait struct？这些用 NTTP 接受 lambda：

```cpp
constexpr auto my_ready = [](auto h) noexcept { return h.done(); };

template<class P, class Pa>
using my_awaitable = build_awaitable_t<P, Pa,
    /* ... */
    /* on_ready<P, Pa, my_ready> 等 */
>;
```

| 组件 | 作用 |
|---|---|
| `awaitable_traits::on_ready<P, Pa, F>` | `await_ready` 调 `F(handle)` |
| `awaitable_traits::on_suspend<P, Pa, F>` | `await_suspend` 调 `F(handle, parent)`，返回值即整体返回值 |
| `awaitable_traits::on_resume<P, Pa, F>` | `await_resume` 调 `F(handle)`，返回值即 co_await 表达式结果 |

注意 `auto F` 是 NTTP，所以 lambda 必须是无捕获或 `consteval`-friendly 的形式。

---

## 14. 工具 awaitable

源码：[awaitable/](../include/nagisa/concurrency/_coroutine/awaitable/)

这些是开箱即用的 awaitable，可以直接出现在 `co_await` 后面，不依赖具体 promise。

### 14.1 `forward<T>(value)`

「我已经有结果了，但还是想走 await 表达式语法」。`await_ready` 直接返回 true，`await_resume` 返回构造时存的值。

源码：[awaitable/forward.h](../include/nagisa/concurrency/_coroutine/awaitable/forward.h)

```cpp
int x = co_await forward<int>(42);   // 等价于 int x = 42; 但走了 co_await
```

主要用来在 `as_awaitable` 里返回一个"立即就绪"的 awaitable。

### 14.2 `from_handle<T>{}`

「我需要在 `await_suspend` 里用父 handle 构造一个 T，然后在 `await_resume` 里把它取出来」。`current_handle` 就是基于它实现的。

源码：[awaitable/from_handle.h](../include/nagisa/concurrency/_coroutine/awaitable/from_handle.h)

```cpp
struct from_handle<T> : std::suspend_always {
    auto await_suspend(auto&& parent) { _result = T(parent); return false; }
    auto await_resume()               { return std::move(_result); }
    union { std::byte _empty; T _result; };
};
```

`await_suspend` 返回 `false` 意为"算了别真的挂"——构造完直接 resume。

### 14.3 `sync_invoke{fn, args...}`

「我想在 `await_resume` 里同步调用一个函数，把结果作为 co_await 的结果」。

源码：[awaitable/sync_invoke.h](../include/nagisa/concurrency/_coroutine/awaitable/sync_invoke.h)

```cpp
int x = co_await sync_invoke{ []{ return 42; } };
```

类似 `forward` 但延迟到 resume 时才求值。

### 14.4 `current_handle<H>()`

「让我拿到调用方的 `coroutine_handle`」。

源码：[awaitable/current_handle.h](../include/nagisa/concurrency/_coroutine/awaitable/current_handle.h)

```cpp
auto h = co_await current_handle();   // h: std::coroutine_handle<>
auto h = co_await current_handle<std::coroutine_handle<my_promise>>();
```

`spawn_impl` 在 [spawn.h:27](../include/nagisa/concurrency/spawn.h) 用它取得自己的 handle 进而拿到 promise。

### 14.5 `environment()`

「让我拿到当前协程的 stdexec env」。

源码：[awaitable/env.h](../include/nagisa/concurrency/_coroutine/awaitable/env.h)

```cpp
auto env = co_await environment();
auto sch = stdexec::get_scheduler(env);
```

依赖父 promise 实现了 `get_env`（如带 `with_scheduler` 的 promise）。

---

## 15. 完整示例：simple_task 的拼装

把上面的全部串起来看看 `simple_task` 是怎么搭出来的：

```cpp
// promise 端（来自 simple_task.h）
template<class Value, bool Throw, class Scheduler,
         intro_type Intro, class StopToken, class Handle>
struct simple_promise
    : std::conditional_t<Intro == intro_type::eager,
                         promises::eager, promises::lazy>   // §3
    , promises::exception<Throw>                            // §4
    , promises::value<Value>                                // §5
    , promises::jump_to_continuation<Handle>                // §7
    , promises::return_object_from_handle</*...*/>          // §6
    , promises::with_scheduler<Scheduler>                   // §9
    , promises::with_stop_token<StopToken>                  // §10
    , promises::with_await_transform<simple_promise>        // §11
{ /* 构造和 get_env */ };

// awaitable 端
template<class P, class Pa>
using simple_awaitable = build_awaitable_t<P, Pa
    , awaitable_traits::ready_if_done          // §3
    , awaitable_traits::capture_scheduler      // §9
    , awaitable_traits::capture_inplace_stop_token  // §10
    , awaitable_traits::this_then_parent       // §7
    , awaitable_traits::run_this               // §12
    , awaitable_traits::release_value          // §5
    , awaitable_traits::rethrow_exception      // §4
    , awaitable_traits::destroy_after_resumed  // §8
>;

template</* ... */>
using simple_task = basic_task<simple_promise</*...*/>, simple_awaitable>;
```

把这张表当配方对照——每一项的"为什么"在这一章对应的小节里。

→ 下一章：[06 · awaitable_trait_combiner 是怎么拼起来的](./06_combiner.md)
