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


## 已知bug
### msvc
#### wd4100 instantiation ICE
- ICE with `/W4` or `/Wall`.
- Compile with `/W3`.
- Compile with `/W4 /wd4100`.

[run_this.cpp](https://godbolt.org/#z:OYLghAFBqd5QCxAYwPYBMCmBRdBLAF1QCcAaPECAMzwBtMA7AQwFtMQByARg9KtQYEAysib0QXACx8BBAKoBnTAAUAHpwAMvAFYgAzKVpMGoZAFIATACFLV0kvrICeAZUboAwqloBXFgwlSdwAZPAZMADk/ACNMYhAAVgTSAAdUBUIXBi9ff0C0jOcBUPCollj4pPtMRyKGIQImYgIcvwCuatqshqaCEsiYuMTkhUbm1ryO0d7%2BsorhgEp7VB9iZHYOSz0w5F8sAGozPQ80BnWUggUj7DMNAEEtnb3MQ%2BO0YhXncOvbh4tts7PV4eKg%2BM51MQ/e6PQE%2BA5HDwEACeKUwAH0CMQmIQrnobtD/k84S8ET5nLRCEioQ97qNiD4nPtkAg6Og0SkPiw8EojjZaZiGQR9ikmowCOzOdzMLzfr8CJgWCkjPLgbsmAoFPtlJKlKQmUYNVrRYJqXTBft6QwMSyrtCAOx8mkATlOo0wqg5%2B2mzmQ%2Bywu2RqIgTDJqAW%2ByYAHdseKFD4FKiGOgILKNE7bk7RugQCgSJ8wuiEMZ0PQEdrUFyeXivTUqKRU%2Bm01mc%2B98%2BE0UWk6XjspjQRrsK%2B%2BGGKh3ed%2B9C02YHQ2M8RMARVgwa7QqDLJ%2Bm7QARWXb9c0u7yxXKklvA2agAqWMIpoFjKjMbRcYT7gxu8dGaPSqYKoRasN5aVpger/pqvbziaeINq68oesQfqYAGKKYMGobhqgqJYkQxAQAsKYbhmza5h8ZIFh2xbdh4gFSgODh1rOTYENmxFtoWFHSj2fYDiKEEEOGMH7COY6YBcDFOvOACOPh4POmqSdJsmHA6%2BxXjGOYPoQT7xomyZ0XqPFigsvJKTuBEzgRabzouxDLqphDqdGmnPjpEB6YOvFGXoH5Tnu9o7l5qaHgq36/me6qah4YhGNE9AgeeAB0iX7HcxDALi%2BJ3DB7qeiGRBMggiEANa4YJo6qOO74NngVBMgIbpwfsEA5kRYQAG6oKIMUcR4kW0NFsXJalCiJfF1xGWZ3mNk68ZhMAFqYHGtBCkcW5ekxOZtaghXorJPhLW%2Bxy9f1wGDWlI0/AFFniQuS5rcxCisOi6oIrtS16u1eDoAO04eD9d3NY9aLPccr0EHqRGtqR7adiW3XMqyEoVjRUEo5N06mc6NQ8hNYlWbdVBiFWaO%2BQ8JO/DNJj7JiMavKtlrWtyf4srQbIckjuruWKiNARdjoU3Noh9XEtMRo5sbaa%2B/bHNTN6o4F3p4MgQManEBAQMyRUItBUVxPWG4Q3mUNsV2cPM6zOocdgevOgbJFfMbsMIgZgjc8jNx4rhnmOhwSy0JwCS8AEHBaKQqCcL91i2F6KxrCS/x6LwBCaD7SwFUwWDxLhpCFfoejxXoBeF0XRcAGyGJwkiB8noecLwCggBopBJ8HPukHAsBIGgip0LrFDqxWKQ9/ErXICkKRoq1XBOmiCRcGiABqQhcAAHGiJdoqoJfSDQS1xPXEDRNX0RhE0SKcDwpDH8wxBIgA8tE2iIc3Idd2wgi3wwtBny3pBYNEPjACOvXbgvAsAsGMMAcQP98DzicHgVqC1q7ukQmSDYF8wjyj9j/Ck0QsQ3y8FgaumI8AsHPrwBBxBojpEwFuBUECKQmGTksKgRg0rzzwJgSMt9ExkJkIIEQYh2BSD4fIJQahq66A6EYRh5hI7WEMHgaI9dIBLAwnUYBAB6EctBUDAFQPsDRvoNHNlkTYawJ5RgGMwpWDICCAC0BiABayAQBojRKPXwCZPFOJcRyESHx1gahIAY7AAAJBQRiyRUDscvAxAB1SuocKHEE%2BoglRnQn5ZDcEmCY7QghJlmIMeIHQCiZAELk/I6QykMEKeUIYUwaiZIED0cY3g2iBAcE0%2BoYw%2BgFjmPU%2BwPSKlTB6bU%2BYXAlgKBjusfQvt/ZVx/mHDg%2BxN6SH2CwBQI99iTydPFWe%2BxF4r3iiXRquBCDBK2HoBYicmGp0wOnIYWcc4F3zsXN5Bcy5YMSUHEOSy64NybkwtuMBEAgE%2BCkMk5BKBd0HvQYgERHqcFWeszZvodl7K4Acpey9jm8EwPgbCn19AiIEeIYR/BBCKBUOoH%2BkjSCRixCkMhcyOAB1ID83gSzb5kghUKVANVkUbK2ei/ZhycUnIgF4bucLDj/AmTcluCwljPLzu8955cODfOrn8%2BwALn4pw1RYBZvza6AsVUsChGRXCSCAA)

[capture_stop_token.cpp](https://godbolt.org/#z:OYLghAFBqd5QCxAYwPYBMCmBRdBLAF1QCcAaPECAMzwBtMA7AQwFtMQByARg9KtQYEAysib0QXACx8BBAKoBnTAAUAHpwAMvAFYgAzKVpMGoZAFIATACFLV0kvrICeAZUboAwqloBXFgwlSdwAZPAZMADk/ACNMYhAAdi5SAAdUBUIXBi9ff0C0jOcBUPCollj4pPtMRyKGIQImYgIcvwDkh0wnLIamghLImLjEjsbm1rzRvoGyipGASntUH2Jkdg5LPTDkXywAajM9DzQGNZSCBUPsMw0AQU3t3cwDo7RiZedwq5v7iy3Tp4vDxUHynOpib53B4Anz7Q5ec5ZCF6a5Qv6PWHPeEEACeKUwAH0CMQmIRLiifj8FMSfE49sgEHR0ASUu8WHglIcbHdqcRaQQ9ikmowCCy2RzMFzKTyaXTqagUgSFMtVpKoQlufcEgARKVQmV8umiWi0aJMZAAa0pGutur0mp%2BBEwLBSRidQJ2TAUCj2QhVa1I9KM3r2HjEpvNVopBv59IE4ScSqIiqIFsY1odGgAnH6Vms9vLFcq85L7dKs9T0CAQArwbR4WGTWbLVcC8mCcaIy2y%2Bq7Znbk6XW6sa9gz7c6rA56Q8pxUop2O9sphYJIbdebHROcVoTC0TUGmGBnyz4MiY9ghjOh6ES8SPtW2qygSB8woTLwxr6WPLPUOzORSPb3Nmp5hMAcaCJgqiiri%2BIvA%2BJwJqKe6pumRwTgG9Lhs2UbXEB5YnNSUGsm2TDOMgexMD4RD0sQmBkYSJxOtBEAfl%2Bt5wQ4VCBsudGrhYABsgorgQ8x7AwqBQWcBDlmYNr6lmNxZnRBArAwEHMTBd5yTYOp6sBil6b2%2BmOs6roMR6i4ACokoQa4bkadEMR2shQTBx4KYO5nuvC04%2Br%2B/6YAuXr%2BSJa5KYRzEkVgOywZgEBUUQYkKnEZEkBA8wQLJ2aVtWbyvuEBJsfQ8IBRKrZcaQ2VZrxIqWEJQp8aJEHUuJkmqNJ1V0QAjj4eB0T6PV9QNBwansNmkgQeVOU6LmQSxlXCU18xcqNuoKTpXWYKpxDqRNhDTfRs1MW5ECLY1Iorfh2ZyetWp9tKA5mcOlkhaG4ZMNE9DBd6AB0/17LcxDAOSqK3JFxHEJR1GoPSCBdBaGVtVJmDnB5Bl4FQLVRVDEDVrlIBhAAbqgohfd%2BjZGOTgZAyD/2/VcK0bfJBlKaB54DT4tACocD4E8TB6Epz3NEg2H3U4DwMKPT3zXYp2YqWpj746whJevCwsEFO8ZdMh7aoUeRx7sWk5YU2kay3hmaGXdSk1JyzPW0piu7XsVBiABTtGfdj3s%2BBxKTfBWHbnRSYKvuh7wsb/pBXDTJin%2BEqBhdggJ4Fsuan7Zv0FDvO0UdjGudBotHAHdmAf21JkXgyAEiFcQEBADII/CBHhnEVUKQT%2BXUW%2BRVXiVryMrQzKsonAHYJ3Bkp6KY/p4JVwZVdmocIstCcAArLwAQcFopCoJwHi2LYBYxwcfx6LwU276vizw0wWDxBlpAWvoei/Xon9f9/38CYYnCSG3poXgB8OC8AUCADQpBr5aEWHAWASA0AujoB3CgTc/wpBQfEImyAUiKiJlwLMBIN5cAJAANSEFwAAHASASBJVACWkDQbmcQIEQGiMA0g0QwhNBxJwHgXCeHEBxAAeWiNoXWnCkFsEECIhgtA%2BE31IFgaIPhgCUwgdwXgWAWDGGAOIJR%2BA6LdCJpgTRe8oJdGousARYQnTryUbQPA0QSTCK8FgThxI8AsH4bwUxxBojpEwNqZ0einEmGAYsKgRgQZkLwJgAA7iI/EO8BH8EECIMQ7ApAyEEIoFQ6glG6GSEYCJ5hrC2EMM4iBkBFi1iyJogA9BJWgqBgCw0aRRRpuVyk2GsMOVqjT8TEH/BkUxABaPYjSABayAQAEg7K6U8SyfQzLmayVG7w1jehIFM7AAAJBQXTqJUHGVQqZAB1QB%2B9/HEDwFgGpz9OjdFcBAdwEwAgWGSCEN8sxhjSAKJkAQHyQBfNSOkIFDAZhDHiNIZ5dRejjG8G0UFHQai6x6GMfovyYUgDhVikFYKq7NGheUf5iwTZrH0GvTeQClGgL2AwyQewWAKBwXsAhWZfokL2BQ6hv0hIQFwIQXZmw9DzCvpEu%2B9FH6UEWK/T%2BH8f7Ks/n/Bx1yd571AeAyB0DImkHgYgGs1EUjUXIJQJBmCc4RFVpwJlLK2UUU5dyrgvLKFUIFbwTA%2BAiB3KrAYdJwhjTZKYbIfJahOHFNIAkkkKRfE0o4FvUgmqQGcBESa6iexUBY3tay9lzqeV8o9YKrwyCc7nz0FwCVeqb7zHlW/JVKqf7/w4Bqzh2r7C6pgbfFtFg6Vas4JK2tix/EZFcJIIAA)

## 贡献与开发
- 欢迎提交 PR、Issue 或性能基准。请在 PR 中包含可复现的最小示例与基准结果。
- 代码风格与提交：遵循仓库现有风格；在提交前运行项目自带的测试/示例并通过静态分析（如有）。

## 进一步阅读
- `stdexec` / `senders/receivers` 概念文档
- 本库示例目录中的 `run_loop.cpp`、`static_thread_pool.cpp`、`benchmark/fibonacci.cpp`（可直接编译运行以验证适配）

## 作者
- `nagisa`

