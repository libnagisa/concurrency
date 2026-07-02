# 04 · task 的解剖

阅读本章后你将能：

- 区分 `basic_task` 与 `basic_fork`，理解它们的所有权语义。
- 知道 `release()` / `handle()` / `co_await` / `as_awaitable` 各自做什么。
- 避免最常见的 task 生命周期错误。

---

## 1. 一切始于一个 handle

C++20 里"协程的返回对象"本质上是个包装类，至少要持有一个 `std::coroutine_handle<P>`。本库提供了两种最小化的包装：

| 类型 | 所有权 | 是否 awaitable | 用途 |
|---|---|---|---|
| `basic_fork<P, A>` | **不** 持有所有权 | 取决于 `A`；带适配器时可 await | 当协程生命周期由别人管时 |
| `basic_task<P, A>` | **持有** 所有权（RAII） | 取决于 `A`；带适配器时可 await（**右值**） | 大多数 task 的基类 |

> `A` 是一个 `template<class Promise, class Parent> class` 模板，通常由 [`build_awaitable_t`](./06_combiner.md) 拼装而成。如果省略（使用默认的 `noop_awaitable`），返回对象就只是个 handle 包装，不能被 `co_await`。

源码：[task.h](../include/nagisa/concurrency/_coroutine/task.h)

---

## 2. `basic_fork`：纯 handle 包装

```cpp
template<class Promise, template<class, class>class Awaitable = noop_awaitable>
struct basic_fork;
```

最常见的写法：

```cpp
basic_fork<my_promise> f = handle;   // 直接拿现成 handle 包一下
auto h = f.handle();                 // 取出来
```

带 awaitable 适配器时，`basic_fork` 可以在一个协程里被反复 `co_await`（因为它不释放所有权）：

```cpp
auto fork = some_fork;
co_await fork;
co_await fork;   // 合法——fork 还活着
```

这种"可重复 await 同一个协程实例"的语义是 fork 区别于 task 的关键。

---

## 3. `basic_task`：RAII 容器

```cpp
template<class Promise, template<class, class>class Awaitable = noop_awaitable>
struct basic_task;
```

`basic_task` 的语义：

| 操作 | 行为 |
|---|---|
| 构造 | 接受一个 handle |
| 拷贝构造 / 拷贝赋值 | **删除**（move-only） |
| 移动构造 | 转移 handle，源置空 |
| 移动赋值 | **删除**（避免静默泄漏旧 handle） |
| 析构 | 若仍持有 handle，调用 `handle.destroy()` |
| `handle()` | 取 handle（不释放所有权） |
| `release()` | 释放所有权，返回原 handle，自身置空 |
| `co_await task`（**仅右值**） | 调用 `release()`，所有权交给 awaiter |
| `as_awaitable(parent)`（**仅右值**） | 同上，但走显式 promise 参数路径 |

### 3.1 为什么 `operator co_await` 是 `&&` 限定的？

因为 await 一个 task 意味着"这个 task 的协程帧从现在起归 awaiter 管"——awaiter 负责让它跑完并最终销毁。如果允许左值 await，原 task 还会试图在析构时再 destroy 一次，造成 double-free。

```cpp
simple_task<int> t = make_task();

co_await std::move(t);   // ✅ 显式表明 move
co_await make_task();    // ✅ 右值，无需 std::move
co_await t;              // ❌ 编译错：const lvalue 不能匹配 && 限定
```

如果你需要"先 co_await，之后还能访问 task"，那应该用 `basic_fork`（或保留 `handle()` 引用）。

### 3.2 `release()` 的正确用法

当你想把协程的所有权交出去——比如 `spawn` 把 task 交给 scheduler，或者你想手动控制销毁时机——用 `release()`：

```cpp
auto t = f1(5);
t.handle().resume();        // 启动它（lazy task 需要手动启动）
auto h = t.release();       // 把所有权拿出来
// ... h 在某处被 destroy()
h.destroy();
```

**`release()` 之后调用方必须保证 handle 最终被销毁**，否则就是协程帧泄漏。常见做法有两种：

1. 让 promise 用 [`exit_then_destroy`](./05_components.md#exit) ——协程跑完 `final_suspend` 时自动 `destroy()`。`spawn` 就是这么做的。
2. 自己存好 handle，在合适时机调 `destroy()`。

---

## 4. 完整生命周期示例

下面这段是 [example/core.cpp:67-87](../example/core.cpp) 的逐行解读：

```cpp
// 模式 A：让 task 自己管生命周期
{
    auto t = f1(5);          // task 持有 handle
    t.handle().resume();     // lazy task 需要手动启动
    // 作用域结束 → ~task() → handle.destroy()
}

// 模式 B：手动 release + 手动 destroy
{
    auto t = f1(5);
    t.handle().resume();     // 启动
    auto h = t.release();    // task 放手
    h.destroy();             // 必须显式销毁，否则泄漏
}

// 模式 C：交给 spawn（detach 启动）
{
    nc::spawn(stdexec::inline_scheduler{}, f1(5));
    // spawn 内部使用了 exit_then_destroy 组件，
    // 协程跑完 final_suspend 时自动销毁。
}
```

---

## 5. 与 awaitable 适配器的关系

`basic_task<P, A>` 的第二个模板参数 `A` 决定它如何被 `co_await`：

```cpp
template<class ParentPromise = void>
using awaitable_type = Awaitable<promise_type, ParentPromise>;
```

`A` 必须是个 `template<class Promise, class Parent> class`，能用 `handle_type` 构造（或用 `(handle_type, ParentPromise&)` 构造）。库里有现成的 `simple_awaitable`，你也可以用 [`build_awaitable_t`](./06_combiner.md) 自己拼一个，挑选不同的行为组合（是否捕获 scheduler、是否抛异常、是否在 resume 时销毁帧 …）。

```cpp
// simple_task 内部就是这么定义的（简化版）
template<class T>
using simple_awaitable = build_awaitable_t<
    /* promise = */ simple_promise<T, /*...*/>,
    /* parent  = */ /* deduced */,
    awaitable_traits::ready_if_done,
    awaitable_traits::capture_scheduler,
    awaitable_traits::capture_inplace_stop_token,
    awaitable_traits::this_then_parent,
    awaitable_traits::run_this,
    awaitable_traits::release_value,
    awaitable_traits::rethrow_exception,
    awaitable_traits::destroy_after_resumed
>;

using simple_task<T> = basic_task<simple_promise<T, ...>, simple_awaitable>;
```

具体每个 trait 的语义看[第 5 章](./05_components.md)；怎么拼起来的看[第 6 章](./06_combiner.md)。

---

## 6. 常见陷阱速查

| 错误 | 现象 | 怎么改 |
|---|---|---|
| 把 lazy task 析构而没启动 | 协程帧分配了又被销毁，什么都没跑 | 启动之 (`handle().resume()`) 或交给 `spawn` |
| `release()` 后忘了 `destroy()` | 内存泄漏 | 用带 `exit_then_destroy` 的 promise（如 `spawn_promise`），或手动 destroy |
| `co_await t;`（左值） | 编译失败 | `co_await std::move(t)` |
| 同一个 task 被 `co_await` 两次 | 编译失败（首次已 release） | 第二次起把它换成 `basic_fork` |
| 多线程同时 `resume()` 同一 handle | 未定义行为 | 用 scheduler 串行化恢复 |

→ 下一章：[05 · 组件库速查](./05_components.md)
