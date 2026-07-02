# 03 · 与 stdexec 集成

阅读本章后你将能：

- 解释 task 和 sender 双向互通的原理。
- 知道 `co_await sender` 背后的 `await_transform` / `as_awaitable`。
- 用 `let_value` / `starts_on` / `when_all` 等 sender 算法和 task 配合。
- 理解 scheduler 和 stop_token 是怎么在 sender 和 coroutine 之间穿越的。

---

## 1. 两个方向

`nagisa.concurrency` 和 stdexec 是**双向**互通的：

```
       co_await
sender ──────────► coroutine          ← 在 task 内部 co_await 任何 sender
       (via with_await_transform + stdexec::as_awaitable)

       sync_wait / let_value / ...
coroutine ──────────────────────► sender   ← task 本身是 sender
       (via stdexec::__as_sender of awaitables)
```

两个方向都不需要用户写转换代码——`simple_task` 已经把所有接缝都装好了。

---

## 2. 方向 A：在 coroutine 里 `co_await` sender

`simple_promise` 装了 [`with_await_transform`](./05_components.md#11-让-sender-可-co_await)，它把所有 `co_await X` 转成 `co_await stdexec::as_awaitable(X, promise)`。`as_awaitable` 会：

- 如果 `X` 已经是 awaitable → 直接返回；
- 如果 `X` 是 sender → 包成一个 awaiter（内部 connect + start，接到 `set_value` 时 resume，接到 `set_error` 时 rethrow，接到 `set_stopped` 时调 promise 的 `unhandled_stopped`）。

所以下面这些写法在 `simple_task` 里都合法：

```cpp
nc::simple_task<void> demo() {
    // 等单值 sender，拿到它的 value
    int x = co_await stdexec::just(42);

    // 等多个 sender 一起完成，拿元组
    auto [a, b, c] = co_await stdexec::when_all(
        stdexec::just(1),
        stdexec::just('x'),
        stdexec::just(3.14)
    );

    // 切到一个 scheduler 上恢复执行
    co_await stdexec::schedule(co_await stdexec::get_scheduler());

    // 查询环境
    auto sch = co_await stdexec::get_scheduler();
    auto tok = co_await stdexec::get_stop_token();
}
```

完整示例：[example/hello_execution.cpp](../example/hello_execution.cpp)、[example/hello_world.cpp](../example/hello_world.cpp)。

---

## 3. 方向 B：把 task 当 sender 用

`simple_task` 实现了 `operator co_await`（右值限定），返回 `simple_awaitable` 这个 awaiter。`as_awaitable` 反过来也认识 awaiter——`stdexec` 内部有 `awaitable_sender` 把它包成 sender。

所以 task 可以直接进 sender pipeline：

```cpp
auto pipe =
      stdexec::schedule(sch)
    | stdexec::let_value([] { return my_task(); })   // 后续接 task
    | stdexec::then([](int x) { return x + 1; });

stdexec::sync_wait(std::move(pipe));
```

也可以直接 `sync_wait(my_task())`，stdexec 会自己把它当 sender。

---

## 4. let_value 接收 task

`let_value` 接受一个上游 sender 和一个**函数**——函数被调用以构造下一段 sender。函数返回 task 完全合法：

```cpp
nc::simple_task<int> compute(int seed) {
    co_return seed * 2;
}

auto pipe =
      stdexec::just(21)
    | stdexec::let_value([](int seed) { return compute(seed); });

auto [n] = stdexec::sync_wait(std::move(pipe)).value();   // 42
```

注意：传入 `let_value` 的函数**每次上游完成时都会重新调一次**，所以每次返回一个**新的** task。不能预先构造一个 task 然后多次喂给同一 `let_value`——task 是 move-only 且 await 一次就消耗掉。

---

## 5. starts_on：在指定 scheduler 上起一段子流水

```cpp
exec::static_thread_pool pool{ 4 };
auto sch = pool.get_scheduler();

auto pipe = stdexec::let_value(stdexec::get_scheduler(), [](auto sched) {
    return stdexec::starts_on(sched, my_task());
});

stdexec::sync_wait(std::move(pipe));
```

`starts_on(sched, sndr)` 等价于"先 `schedule(sched)` 再 `then(...)`"，但它有额外语义：会把 `sched` 也设到 sender 的 attribute 里，下游能查到。

---

## 6. scheduler 是怎么穿越的

```
┌──────────────────────────────────────┐
│ stdexec::sync_wait                   │ ← 提供一个内置的 run_loop scheduler
└──────────────────┬───────────────────┘
                   │ 通过 receiver env
                   ▼
┌──────────────────────────────────────┐
│ simple_task                          │
│  promise 实现 get_env()              │ ← 暴露给 stdexec
│  promise 里有 with_scheduler<S>      │
└──────────────────┬───────────────────┘
                   │ co_await child_task 时
                   │ capture_scheduler 把父 scheduler 复制给子
                   ▼
┌──────────────────────────────────────┐
│ child simple_task                    │
│  内部 co_await stdexec::get_scheduler│ ← 拿到的就是上游传下来的
└──────────────────────────────────────┘
```

也就是说，只要一条调用链上都是 `simple_task`，scheduler 自动一路传下去。第一个 task 的 scheduler 来自上游 sender 或 `sync_wait` 的内置 `run_loop`。

stop_token 走的是完全对称的路径，由 `with_stop_token` ↔ `capture_inplace_stop_token` 这对组件承担。

---

## 7. when_all 的特殊处理

```cpp
nc::simple_task<void> all() {
    co_await stdexec::when_all(
        stdexec::just(42),
        stdexec::get_scheduler(),
        stdexec::get_stop_token()
    );
}
```

`when_all` 在 await 时会给每个子 sender 提供同一个 receiver 的 env——所以子 sender 里的 `get_scheduler` / `get_stop_token` 拿到的都是父 coroutine 的环境。把它当成"并发 fork-join"用就行。

完整示例：[example/hello_world.cpp:22-25](../example/hello_world.cpp)。

---

## 8. 把 task 当成另一种 sender：`stopped_as_optional`

stdexec 算法对 task 一视同仁，所以可以正常套：

```cpp
auto with_cancel = nc::simple_task<std::optional<int>>([]() -> nc::simple_task<...> {
    co_return co_await stdexec::stopped_as_optional(some_task());
}());
```

`stopped_as_optional` 把"`set_stopped`"转成"`set_value(nullopt)`"，所以可以把取消语义降级成普通的可空结果。具体见 [example/hello_execution.cpp](../example/hello_execution.cpp)。

---

## 9. 何时**不**要用 task

- **纯算法管道**：单纯的 `then` / `let_value` / `when_all` 组合，写 sender 比写 task 更直接，性能也常更好（不分配协程帧）。
- **需要静态可分析的完成签名**：sender 的 `completion_signatures` 是编译期可枚举的，task 的不是（要看 `co_await` 了哪些东西）。
- **大量小并发任务**：task 每个都要分配协程帧，sender 通常可以拼成单个 operation_state。

反过来，task 在**复杂控制流**（if / loop / 早返回 / try-catch）下比 sender 拼装可读得多。混用是常态——通常顶层一个 task，里面 await 各种 sender 算法。

---

## 10. 常见坑

| 现象 | 原因 | 解 |
|---|---|---|
| `co_await sndr` 编译失败 | 你的 task 类型 promise 没装 `with_await_transform` | 加上它；或换用 `simple_task` |
| `co_await stdexec::get_scheduler()` 抛 | 上游没人提供 scheduler | `sync_wait` 会兜底；自己 connect 的话要在 env 里提供 |
| `let_value([] { return task; })` 第二次执行行为奇怪 | task 是 move-only 且 await 即消耗 | 每次返回新 task，别捕获共享 |
| 嵌套 task 拿不到父 scheduler | 内层 task 的 promise 没装 `with_scheduler<...>` / 父没装 `capture_scheduler` 对应的对面 | 走 `simple_task` 默认配置，或参考[第 5 章](./05_components.md)对照配齐 |

→ 下一章：[04 · task 的解剖](./04_task_anatomy.md)
