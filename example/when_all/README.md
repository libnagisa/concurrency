# when_all_range 示例

`nagisa::concurrency::when_all_range(range)` 启动运行时数量的同类型子 sender，等所有子任务完成后以 `set_value()` 完成。输入必须是可取大小的 range；子 sender 的成功完成不带值，错误通道使用 `std::exception_ptr`。需要结果时，可在子任务中写入各自的结果槽。

| 文件 | 演示内容 | 预期结果 |
| --- | --- | --- |
| `range.cpp` | `let_value` + `iota/transform` 动态生成任务，再用 `then` 汇总 | 总和 15 |
| `parallel.cpp` | 通过线程池调度子任务，分别写入结果槽，等待全部完成后读取 | 平方和 204 |
| `co_await.cpp` | 在 `exec::task` 内等待 `vector<exec::task<void>>` | 总和 100 |
| `error.cpp` | 子任务抛异常，等待剩余任务完成后由 `sync_wait` 重新抛出 | 捕获 `item 1 failed`，其余 3 项完成 |

`when_all_range` 本身不创建线程；并行执行由子 sender 的 scheduler 决定。子任务发生错误或停止时，会请求其他子任务停止，但仍等待所有任务完成；不响应取消的任务会继续执行。错误优先于取消。

右值容器可以通过 `std::move` 转移给聚合 sender；左值容器和 `span` 是借用，连接时仍会移动其元素，因此应将子 sender 视为一次性消费。借用的容器至少要活到连接完成；子任务引用的结果数据和 scheduler 等资源应活到任务完成。已保存的聚合 sender 使用 `std::move` 交给 `sync_wait`。

协程示例使用 stdexec 的 `exec::task`，与当前测试工具链兼容；没有使用本地存在独立适配问题的 `nagisa::concurrency::simple_task`。

完成仓库 CMake 配置后，在开发者命令行运行：

```powershell
cmake --build build --parallel --target example_when_all_range example_when_all_parallel example_when_all_co_await example_when_all_error
ctest --test-dir build --output-on-failure -R '^example\.when_all\.'
```

示例采用 C++23（实现使用 `std::views::zip`）。启用 `BUILD_TESTING` 时，四个示例自动加入 CTest；也可直接运行对应可执行文件。
