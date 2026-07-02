# nagisa.concurrency 文档

`nagisa.concurrency` 是一组**可组合的 C++20 协程零件**，目标是让用户能像搭积木一样拼出自己想要的协程类型，并与 [`stdexec`](https://github.com/NVIDIA/stdexec)（P2300 sender/receiver）双向集成。

本目录是面向使用者的中文文档；源码内的 API 速查请看头文件里的 Doxygen 注释（英文）。两者互补——本文档负责"为什么、怎么用、怎么组合"，源码注释负责"每个符号的精确语义"。

## 文档地图

> 标 ✅ 的章节属于**上手指南**，按顺序读完即可日常使用。
> 标 🚧 的章节属于**完整手册**的进阶部分，尚未完成。

| 章节 | 主题 | 状态 |
|---|---|---|
| [00_overview](./00_overview.md) | 设计哲学与整体地图 | ✅ |
| [01_concepts](./01_concepts.md) | C++20 协程速览，库的核心 concept | ✅ |
| [02_quick_start](./02_quick_start.md) | 用 `simple_task` + `spawn` 起步 | ✅ |
| [03_with_stdexec](./03_with_stdexec.md) | 与 stdexec 双向互通 | ✅ |
| [04_task_anatomy](./04_task_anatomy.md) | `basic_task` 的所有权与生命周期 | ✅ |
| [05_components](./05_components.md) | promise/trait 组件库速查表 | ✅ |
| [06_combiner](./06_combiner.md) | `awaitable_trait_combiner` 怎么把零件拼起来 | ✅ |
| [07_custom_task](./07_custom_task.md) | Cookbook：自己搭一个 task 类型 | ✅ |
| 08_scheduler | 自定义 scheduler 与 `any_scheduler` 类型擦除 | 🚧 |
| 09_stop_token | 停止语义、stop_token 透传链 | 🚧 |
| 10_pitfalls | 常见坑（lazy 启动、release 后的 destroy 责任、生命周期） | 🚧 |

## 阅读建议

- **只想用现成 task**：读 02 → 03 即可
- **想理解为什么 task 是这样**：02 → 04 → 06
- **想自己搭一个 task 类型**：02 → 04 → 05 → 06 → 07
- **想为本库写新组件**：通读 01–07，重点读 06

## 示例代码

所有示例代码在 [`../example/`](../example/) 目录下，可直接编译运行。本文档中的代码片段都标注了来源文件和行号。

## 反馈

文档错误、不清楚的地方、或希望补充的主题，欢迎提 Issue 或 PR。
