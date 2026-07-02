# 01 · 协程概念

阅读本章后你将能：

- 区分 **awaiter / awaitable / promise / coroutine_handle** 四个最基础的概念，知道 `co_await x` 背后到底发生了什么。
- 看懂本库源码里反复出现的 `awaiter`、`awaitable`、`promise`、`coroutine_handle`、`await_suspend_result` 几个 concept。

如果你已经熟悉 C++20 协程，可以直接跳到[第 2 章](./02_quick_start.md)。

---

## 1. 协程的"三件套"

C++20 的协程是一种**可挂起、可恢复的函数**。一个协程要跑起来，需要三种东西配合：

```
┌──────────────────────┐
│ 协程函数本体          │  你写的那个 co_await / co_return / co_yield 的函数
│  (coroutine body)    │
└──────────┬───────────┘
           │ 编译器自动生成
           ▼
┌──────────────────────┐
│ promise_type         │  描述协程"自己怎么活"
│                      │  initial/final_suspend、return_value、unhandled_exception ...
└──────────┬───────────┘
           │ 每次 co_await 时
           ▼
┌──────────────────────┐
│ awaiter              │  描述"暂停一下我去等点什么，等完了给我什么结果"
│                      │  await_ready / await_suspend / await_resume
└──────────────────────┘
```

第四个角色是 **`coroutine_handle<P>`**——指向一个具体协程实例的句柄，可以 `resume()`、`destroy()`、查询 `done()`、或者拿到它的 `promise()`。

---

## 2. `co_await x` 拆解

写一句 `co_await x`，编译器实际做的是：

```cpp
// 伪代码
auto&& awaitable = x;
auto&& awaiter   = get_awaiter(awaitable, this_promise);   // ① 取 awaiter

if (!awaiter.await_ready()) {                              // ② 问要不要挂起
    auto suspend_result = awaiter.await_suspend(this_h);   // ③ 挂起，告诉它接下来谁跑
    // 根据 suspend_result 的类型决定：
    //   void              -> 真挂起
    //   bool              -> false 表示"算了别挂"
    //   coroutine_handle  -> 对称转移到该协程
}

decltype(auto) result = awaiter.await_resume();            // ④ 恢复后产出 co_await 的结果
```

注意两个不同的概念：

| 概念 | 角色 |
|---|---|
| **awaitable** | 出现在 `co_await` 后面的东西。可以是 awaiter 本身、可以带 `operator co_await`、也可以通过 promise 的 `await_transform` 转换。 |
| **awaiter** | 真正被询问的那个对象——必须有 `await_ready / await_suspend / await_resume`。 |

一个 awaitable 通过最多两步转换变成 awaiter：

```
awaitable
  │ (1) promise.await_transform(awaitable)  ← 可选，只在协程内 co_await 时
  ▼
some_intermediate
  │ (2) operator co_await(some_intermediate)  ← 可选
  ▼
awaiter
```

源码中的 [`get_awaiter`](../include/nagisa/concurrency/_coroutine/awaitable.h) 函数完整模拟了这两步。

---

## 3. 本库的 4 个核心 concept

### 3.1 `await_suspend_result`

`await_suspend` 的返回类型必须是三种之一：

```cpp
template <class T>
concept await_suspend_result =
    ::std::same_as<T, void>   // 无条件挂起
 || ::std::same_as<T, bool>   // true=挂，false=不挂
 || coroutine_handle<T>;      // 对称转移：直接跳到这个 handle
```

源码：[awaiter.h:8](../include/nagisa/concurrency/_coroutine/awaiter.h)

**对称转移**是高性能协程的关键——它避免了"挂起当前 → 调度器选下一个 → 跳过去"的间接开销，直接把控制权一脚踢到目标 handle。本库的 `jump_to_continuation` 等组件都用它。

### 3.2 `awaiter<A, P = void>`

判断 `A` 是不是一个合法的 awaiter，可被一个 promise 类型为 `P` 的协程 await：

```cpp
template <class Awaiter, class ParentPromise = void>
concept awaiter = requires(Awaiter& a, std::coroutine_handle<ParentPromise> h) {
    a.await_ready() ? 1 : 0;
    { a.await_suspend(h) } -> await_suspend_result;
    a.await_resume();
};
```

源码：[awaiter.h:11](../include/nagisa/concurrency/_coroutine/awaiter.h)

`P = void` 意味着 await_suspend 接受的是类型擦除的 `std::coroutine_handle<>`。

### 3.3 `awaitable<A, P = void>`

判断 `A` 是不是可以写在 `co_await` 后面：

```cpp
template <class Awaitable, class ParentPromise = void>
concept awaitable =
    // ParentPromise 是 void 时，跳过 await_transform 步骤
    (std::same_as<ParentPromise, void> && requires(Awaitable&& a) {
        { get_awaiter(static_cast<Awaitable&&>(a)) } -> awaiter<ParentPromise>;
    })
    // 否则，先经过 promise.await_transform
 || requires(Awaitable&& a, ParentPromise& p) {
        { get_awaiter(static_cast<Awaitable&&>(a), p) } -> awaiter<ParentPromise>;
    };
```

源码：[awaitable.h:51](../include/nagisa/concurrency/_coroutine/awaitable.h)

配套有两个 helper：

| 工具 | 作用 |
|---|---|
| `get_awaiter(a)` | 不经过 promise，把 `a` 转成 awaiter |
| `get_awaiter(a, p)` | 经过 `p.await_transform`（如果有）后再转 |
| `awaitable_awaiter_t<A, P>` | 推导 `A` 在 promise `P` 下的 awaiter 类型 |
| `awaiter_result_t<Aw, P>` | 推导 awaiter 的 `await_resume` 返回类型 |

### 3.4 `promise<P>`

判断 `P` 是不是合法的 promise：

```cpp
template<class T>
concept promise =
    std::same_as<T, std::remove_reference_t<T>>
 && requires(T& p) {
        { p.initial_suspend() } -> awaitable;
        { p.final_suspend()   } -> awaitable;
        { p.get_return_object() };
        { p.unhandled_exception() };
    };
```

源码：[promise.h:8](../include/nagisa/concurrency/_coroutine/promise.h)

⚠️ 注意 concept **没有**强制要求 `return_value` 或 `return_void`——这俩具体要哪个由协程函数体决定，编译器在实例化时才查。

### 3.5 `coroutine_handle<T>`

判断 `T` 是不是某个 `std::coroutine_handle<P>` 的特化：

```cpp
template<class T>
concept coroutine_handle = requires(T t) {
    coroutine_detail::is_derived_from_std_coroutine_handle(t);
};
```

源码：[coroutine_handle.h:20](../include/nagisa/concurrency/_coroutine/coroutine_handle.h)

它的配套工具是 `coroutine_handle_cast<To>(from)`——把 `coroutine_handle<>` 转成 `coroutine_handle<P>` 之类，类似 `from_address` 但带类型检查（注意：**只检查目标类型形状合法，不检查运行时 promise 真的是这个类型**）。

---

## 4. 小结：本库的 concept 全图

```
         coroutine_handle<T>
                │
                │ (作为 await_suspend 的返回值之一)
                ▼
        await_suspend_result<T>
                │
                ▼
            awaiter<A, P>          ← co_await 真正驱动的对象
                │
                │ get_awaiter(a [, p])
                ▼
           awaitable<A, P>         ← co_await 可以接受的东西

            promise<P>             ← 协程编译器约束
```

掌握这五个 concept 后，你就能看懂库的所有签名了。下一章我们用 `simple_task` 把它们运行起来。

→ 下一章：[02 · 快速上手](./02_quick_start.md)
