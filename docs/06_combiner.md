# 06 · awaitable_trait_combiner：把零件拼成 awaiter

阅读本章后你将能：

- 解释一个"trait"是什么、和"组件"的关系。
- 看懂 `build_awaitable_t<P, Parent, Traits...>` 的展开过程。
- 知道两个 trait 定义同名 hook 时是怎么合并的。
- 自己写一个新 trait 并加入组合。

---

## 1. 为什么要组合机制？

C++20 协程里一个 awaiter 必须实现三件套：

```cpp
struct my_awaiter {
    bool                       await_ready();
    void/bool/coroutine_handle await_suspend(coroutine_handle<P>);
    decltype(auto)             await_resume();
};
```

大多数行为是**正交且常用**的：

- 「ready 时机」——直接 ready / 永远不 ready / 协程 done 才 ready
- 「挂起时要做什么」——通知父协程、捕获 scheduler、捕获 stop token …
- 「resume 时要做什么」——返回值、抛异常、销毁帧 …

如果每写一个新 task 类型都手动写这些 hook，代码重复且容易写错。`awaitable_trait_combiner` 的作用就是让你把每种行为**单独写成一个 trait**，然后用模板把它们粘成一个完整的 awaiter。

---

## 2. trait 长什么样

一个 trait 是个 `template<class Promise, class ParentPromise> struct`，里面**可选**地定义下面这些静态成员：

```cpp
template<class Promise, class ParentPromise>
struct example_trait
{
    using handle_type = std::coroutine_handle<Promise>;

    // 可选：每个 awaiter 实例的状态，在 awaiter 构造时初始化
    static auto create_context(handle_type self);
    static auto create_context(handle_type self, ParentPromise& parent);

    // 可选：三件套之一或多个
    static bool                  await_ready  (/* context, */ handle_type self);
    static auto                  await_suspend(/* context, */ handle_type self,
                                               std::coroutine_handle<P> parent);
    static decltype(auto)        await_resume (/* context, */ handle_type self);
};
```

每个 hook 都可以**选择是否带 context** 参数；combiner 会用 SFINAE 自动挑正确的重载。

库里已有的 trait 都在 `nc::awaitable_traits` 命名空间下，详见[第 5 章](./05_components.md)。

---

## 3. 一次完整展开

来看 [simple_task](../include/nagisa/concurrency/simple_task.h) 内部用的：

```cpp
template<class Promise, class Parent>
using simple_awaitable = build_awaitable_t<Promise, Parent
    , awaitable_traits::ready_if_done           // ① await_ready
    , awaitable_traits::capture_scheduler       // ② await_suspend
    , awaitable_traits::capture_inplace_stop_token  // ② await_suspend
    , awaitable_traits::this_then_parent        // ② await_suspend
    , awaitable_traits::run_this                // ② await_suspend
    , awaitable_traits::release_value           // ③ await_resume
    , awaitable_traits::rethrow_exception       // ③ await_resume
    , awaitable_traits::destroy_after_resumed   // ③ await_resume
>;
```

`build_awaitable_t<P, Parent, T1, T2, ...>` 展开为：

```cpp
awaitable_trait_instance_t<
    P,
    Parent,
    awaitable_trait_combiner<T1, T2, ...>::template type
>
```

其中 `awaitable_trait_combiner` 把 8 个 trait 折叠成 1 个聚合 trait：

```
combiner<T1, T2, T3, ..., T8>
  ≡ combiner<combiner<T1, T2>::type, T3, ..., T8>
  ≡ combiner<combiner<combiner<T1, T2>::type, T3>::type, T4, ..., T8>
  ≡ ...一直折叠到只剩一个
```

每一步两两合并的语义见下一节。

最后 `awaitable_trait_instance_t` 把这个合并后的 trait 包成一个真正的 awaiter 对象——它有 `_coroutine` 和 `_context` 两个成员，三件套 hook 直接转发到 trait。

---

## 4. 同名 hook 怎么合并？

两个 trait 都定义了 `await_suspend`，会怎么样？源码里 `compatible_invoke` 的规则是：

| 两者返回类型 | 怎么合并 |
|---|---|
| 都是 `void` | 都调用。第二个用 `scope_guard` 包，保证即使第一个抛异常也跑。 |
| 一个 `void`、一个非 `void` | 两个都调用；非 `void` 的结果作为整体返回值；void 的当成副作用。 |
| 都非 `void` | **静态断言失败**，不能合并。 |

这套规则的实际效果是：

- `capture_scheduler` / `capture_inplace_stop_token` / `this_then_parent` 三个 `await_suspend` 都返回 `void`，它们的副作用会被叠加；
- `run_this` 的 `await_suspend` 返回 `coroutine_handle`（对称转移到子协程），它的返回值作为最终结果。

context 也会按 trait 顺序拼成 `std::tuple`，hook 里通过 `at_details::context_left/right` 取出自己那一份，对调用者完全透明。

---

## 5. 一个简化的展开例子

假设只有两个 trait，一个有 context、一个没有：

```cpp
template<class P, class Pa> struct A {
    static int  create_context(coroutine_handle<P>);   // context = int
    static void await_suspend(int& ctx, coroutine_handle<P>, auto) { ctx++; }
};

template<class P, class Pa> struct B {
    static auto await_suspend(coroutine_handle<P> self, auto)
        -> coroutine_handle<> { return self; }
};
```

`build_awaitable_t<P, void, A, B>` 大致等价于：

```cpp
struct merged_awaiter {
    int                       _context;   // 来自 A
    std::coroutine_handle<P>  _coroutine;

    bool await_ready() const noexcept { return false; }  // 都没定义 → 默认 false

    auto await_suspend(std::coroutine_handle<> parent) noexcept {
        A::await_suspend(_context, _coroutine, parent);  // 副作用：++ctx
        return B::await_suspend(_coroutine, parent);     // 返回 handle，对称转移
    }

    void await_resume() noexcept {}   // 都没定义 → 空
};
```

实际类型当然比这复杂得多（SFINAE 路径、tuple 拼装），但语义就是这样。

---

## 6. 写一个新 trait

需求："每次 await 都打印一条日志。"

```cpp
template<class Promise, class ParentPromise>
struct log_on_suspend {
    using handle_type = std::coroutine_handle<Promise>;

    static void await_suspend(handle_type self,
                              std::coroutine_handle<ParentPromise>) noexcept
    {
        std::cout << "[log] suspending coroutine @ "
                  << self.address() << '\n';
    }
};

// 接入
template<class P, class Pa>
using my_awaitable = nc::build_awaitable_t<P, Pa,
      nc::awaitable_traits::ready_if_done
    , log_on_suspend                                  // ← 新加的
    , nc::awaitable_traits::this_then_parent
    , nc::awaitable_traits::run_this
    , nc::awaitable_traits::release_value
    , nc::awaitable_traits::rethrow_exception
    , nc::awaitable_traits::destroy_after_resumed
>;
```

因为 `log_on_suspend::await_suspend` 返回 `void`，它会被自动叠在 `this_then_parent` / `run_this` 之前作为副作用。

---

## 7. 关于 `awaitable_trait_combiner` 的几个变体

| 名字 | 形状 | 用途 |
|---|---|---|
| `awaitable_trait_combiner<Ts...>::template type<P, Pa>` | 模板 | 需要把"合并后的 trait"再当模板传 |
| `awaitable_trait_combiner_t<P, Pa, Ts...>` | 已实例化的类型 | 已知 P/Pa 时拿合并后的具体 trait 类型 |
| `build_awaitable_t<P, Pa, Ts...>` | 已实例化的类型 | **最常用**：直接拿装好的 awaiter 类型 |
| `build_awaitable<Ts...>::template type<P, Pa>` | 模板 | 用作 `basic_task` 的 `Awaitable` 模板参数 |

经验法则：

- 在 `basic_task<P, A>` 里要的 `A` 是 `template<class, class>` 形式 → 用 `build_awaitable<Ts...>::template type` 或者直接定义一个 alias 模板。
- 临时拼一个 awaiter 类型 → 用 `build_awaitable_t<P, Pa, Ts...>`。

---

## 8. 设计哲学小结

这套组合机制是**正交分离**思想的极致体现：

- 协程的"自我行为"被拆成 [promise 组件](./05_components.md#promise-组件)。
- 协程的"被等待行为"被拆成 [awaitable_trait 组件](./05_components.md#awaitable_trait-组件)。
- 两套组件之间往往**成对出现**（如 `with_scheduler` ↔ `capture_scheduler`）——一边在 promise 上准备好"我能接收一个 scheduler"，另一边在 await 时主动把父协程的 scheduler 塞过来。

这种设计让"我想要一个 lazy + 能捕获 stop_token + 返回值 + 抛异常的 task"变成一行 trait 列表，而不是从头写一遍 awaiter。

→ 下一章：[07 · Cookbook：自定义 task 类型](./07_custom_task.md)
