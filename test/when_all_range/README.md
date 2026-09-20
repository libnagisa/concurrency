# when_all_range 测试场景

测试只通过 `<nagisa/concurrency/when_all_range.h>` 暴露的接口连接、启动和观察 sender，不访问库的私有状态。共 31 个 `TEST_CASE`，按功能分为 7 个可执行文件；部分用例另有子场景或多组参数。

## 基本行为与范围类型（basic.cpp）

1. 空 `vector` 成功完成，结果为 `optional<tuple<>>`，不携带子任务返回值。
2. 单个、8 个和 256 个同步子任务均恰好执行一次。
3. 惰性 `transform_view` 在 connect 时生成子 sender，在 start 时才执行任务。
4. 右值 `vector` 转移所有权，原容器离开作用域后仍可执行其中只移动的子 sender。
5. 左值 `array` 和借用的 `span` 均可承载只移动的子 sender，并在连接时消费元素。
6. `let_value` 根据运行时数量生成任务，后续 `then` 只能在全部完成后执行。

## 完成与错误（completion.cpp）

7. 全部子任务都会启动；乱序完成时，聚合 sender 等到最后一个完成才通知 receiver。
8. 子任务错误触发兄弟任务取消；不响应取消的任务仍必须完成后才能向外报告错误。
9. `error` 优先于 `stopped`，覆盖两种到达顺序。
10. 多个子任务依次失败时保留最先收到的异常对象。
11. 第一个子任务同步失败后，仍启动其余子任务，并向它们提供已停止的 token。
12. `sync_wait` 重新抛出子任务的原始异常类型和消息。

## 取消（cancellation.cpp）

13. 空范围在外部 token 已停止时返回 `stopped`。
14. start 前已取消、等待期间取消，均向所有子任务传递停止请求；覆盖取消回调内同步完成及重复停止请求。
15. 外部取消不会提前结束不响应取消的子任务；即使该子任务最终成功，聚合结果仍为 `stopped`。
16. 子任务 `stopped` 取消兄弟任务，但不会反向停止外部 stop source。
17. 外部取消之后发生的子任务错误仍以 `error` 完成。
18. 聚合操作完成并销毁后，外部停止请求不会再次调用 receiver。
19. `sync_wait` 将子任务 `stopped` 映射为无值的 optional。

## 生命周期与异常清理（lifetime.cpp）

20. operation state 不可复制或移动；只 connect 而不 start 时也能正确析构所有子操作。
21. 首个、中间或最后一个子任务 connect 抛异常时，恰好销毁此前已成功构造的子操作，且没有任务被启动。
22. 惰性范围求值抛异常时，同样清理此前的连接。
23. 完成通知不会提前销毁子操作；父 operation 析构时，各子操作恰好析构一次。

## 环境（environment.cpp）

24. 子任务共享可停止的内部 token，且它不同于外部 token。
25. 覆盖 stop token 时仍转发外部 scheduler；支持必须在 receiver 环境中解析的子 sender。

## 并发（concurrent.cpp）

26. 8 个线程同时完成子任务，所有写入在聚合完成时可见，receiver 恰好完成一次。
27. 外部取消与最后一个子任务完成竞争，执行 256 轮，每轮恰好完成一次；允许由竞争先后决定 `value` 或 `stopped`。
28. 多线程同时报告错误时只完成一次，报告的异常来自实际子任务。

这些并发用例使用 `barrier` 协调线程，不依赖 `sleep`；竞争测试是回归检查，不保证穷尽所有线程交错。每个 CTest 设置 30 秒超时，避免死锁无限阻塞。

## 协程互操作（co_await.cpp）

29. `exec::task` 可 `co_await` 动态集合中的只移动子协程任务，并在全部完成后继续。
30. 子任务错误可在 `co_await` 处捕获。
31. 子任务 `stopped` 向上传播，不继续执行 `co_await` 后的协程体。

这里使用 stdexec 的 `exec::task`。当前本地 `nagisa::concurrency::simple_task` 与 stdexec 的协程适配存在独立兼容问题，本组测试不覆盖该组合。

## 构建与运行

沿用仓库的 doctest、stdexec 和 CMake 工具链配置。新目标使用 C++23，因为 `when_all_range` 的实现需要 `std::views::zip`。MSVC 请在 x64 Developer PowerShell / Command Prompt 中执行。

```powershell
cmake --build build --parallel --target when_all_range_basic when_all_range_completion when_all_range_cancellation when_all_range_lifetime when_all_range_environment when_all_range_concurrent when_all_range_co_await
ctest --test-dir build --output-on-failure -R '^when_all_range\.'
```

首次配置需要启用 `BUILD_TESTING`、让 `find_package(doctest CONFIG)` 能找到 doctest，并通过 `STDEXEC_INCLUDE_DIR` 指定包含 `stdexec/execution.hpp` 的目录，例如：

```powershell
cmake -S . -B build -DBUILD_TESTING=ON -DSTDEXEC_INCLUDE_DIR=D:/project/stdexec/include
```

若 doctest 通过 vcpkg 安装，在首次配置时同时传入对应的 `CMAKE_TOOLCHAIN_FILE`。
