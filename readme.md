# nagisa.concurrency

## 简要说明

本目录包含 `nagisa` 项目中与并发/调度相关的实现：

* 协程支持:
	1. `awaitable_trait` 适配

代码以现代 C++（`C++20`+）风格为主，目标是与 `execution`(p2300) 无缝集成。

## 许可证

## 目录概览（重要文件）
- `include/nagisa/concurrency/` — 库头文件（协程 promise/awaiter/traits、scheduler/环境适配等）。
- `example/` — 若干示例与基准程序：
  - `hello_execution.cpp`、`hello_world.cpp`：简单示例。
  - `run_loop.cpp`：用于本地 `run_loop` 驱动的协程示例。
  - `static_thread_pool.cpp`、`timer.cpp`：演示如何把任务调度到 `exec::static_thread_pool`。
  - `benchmark/fibonacci.cpp`：并行斐波那契基准（可与 `static_thread_pool` 或 TBB 线程池一起测试）。
- `readme.md`（本文件）

## 依赖

* `stdexec`: https://github.com/NVIDIA/stdexec.git

## 快速开始（开发者）

1. 克隆仓库：
```shell
git clone https://github.com/libnagisa/nagisa.git
cd /path/to/nagisa/libs
# 此项目依赖build_lib
git clone https://github.com/libnagisa/build_lib.git
git clone https://github.com/libnagisa/concurrency.git
```
2. 构建：

	此项目为纯头库，无需构建

3. 运行示例：



## 设计要点
- 协程适配：
  - 提供可复用的 `promise` 组件（例如 `lazy`/`value`/`exception`/`with_scheduler` 等），便于按需组合出不同语义的协程类型（`simple_promise`、`spawn_promise` 等）。
  - 提供可复用的 `awaitable_trait` 组件，可用于简化描述 `task` 作为 `awaitable` 时的行为
  - 完全支持 `execution` ，其所有组件都可与此库交互使用
- 所有权与生命周期：
	```
	```

## 使用示例

基本用法：
```cpp
// example/core.cpp

constexpr auto check_stop() noexcept { return check_stop_t{}; }
task f1(int i) noexcept
{
	::std::cout << i << ::std::endl;
	co_await ::check_stop();
	co_await ::stdexec::schedule(co_await ::stdexec::get_scheduler());
	if (!i)
		co_return;
	co_await f1(i - 1);
}

int main()
{
	{
		auto t = f1(5);
		// lazy start
		t.handle().resume();
		// coroutine will be destroyed when task destruct
		// t.~task();
	}
	{
		auto t = f1(5);
		// lazy start
		t.handle().resume();
		// release the ownership
		auto h = t.release();
		// must destroy the coroutine
		h.destroy();
	}
	{
		::nc::spawn(::stdexec::inline_scheduler{}, f1(5));
	}
	return 0;
}
```

## 常见问题与注意事项


## 贡献与开发
- 欢迎提交 PR、Issue 或性能基准。请在 PR 中包含可复现的最小示例与基准结果。
- 代码风格与提交：遵循仓库现有风格；在提交前运行项目自带的测试/示例并通过静态分析（如有）。

## 进一步阅读
- `stdexec` / `senders/receivers` 概念文档
- 本库示例目录中的 `run_loop.cpp`、`static_thread_pool.cpp`、`benchmark/fibonacci.cpp`（可直接编译运行以验证适配）

## 作者
- `nagisa`

