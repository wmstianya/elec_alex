# 分机小屏实时状态 Implementation Plan

> **For agentic workers:** Use subagent-driven-development for the independent protocol core, then review the integrated implementation. Keep legacy file encodings.

**Goal:** 按用户的 MCGS CSV 接入分机只读实时显示，并提交独立 PR；显示压力、水位、报警、泵、排污阀与三组加热输出。

**Architecture:** 保持原 50 个 holding registers 的位置，小屏作为 Modbus 主站轮询，分机只响应。新接口明确按寄存器序列化，不直接发送 C 联合体；普通字高字节在前，浮点 IEEE754 高字在前（ABCD），在接线文档给出验证帧。新增通道只读，不抢占联控、不改控制命令和参数。串口分配必须依据现场是否保留可控硅调功器；已发出必要的接线澄清，在答复前先完成与端口无关的协议模块。

**Tech Stack:** STM32F103C8T6 / SPL / C90-compatible production C / bounded DMA Modbus RTU / native GCC tests / ARM GCC object checks.

## Task 1: 独立协议与反例

- [x] 新建 `UART/SYSTEM/serial_dma/small_screen_core.c/.h` 与 `UART/tests/test_small_screen_core.c`。固定 34 个 uint16 和 8 个 float 的快照，寄存器共 50 个；API `SmallScreen_Reply(snapshot, station, request, length, output, capacity)` 返回应答长度或 0，无全局控制副作用。
- [x] 先写失败测试：03 读泵/排污状态、逐个加热状态、压力 1.25 的 ABCD 字节、全量 50 字、最后一字、越界、数量 0/126、坏 CRC、短帧、错误站号、广播、06/10 写禁止、输出缓冲不足与连续状态改变。
- [x] 实现后运行严格 native GCC 测试；CRC 使用现有 `ModbusFrame_Crc`，异常 01/02/03，禁止借读操作续期联控或写入任何控制值。

## Task 2: 实时快照与串口归属

- [x] 新建分机适配模块，控制输出执行后采集泵/排污/三组加热的 `Switch_Inf` 标志；寄存器 23 是输出位图（bit0 泵、bit1 排污、bit2～4 加热1～3），24～26 分别是加热1～3，27 为投入组数，28 为排污阶段，29 为阀位反馈（UNKNOWN 保持未知），其余原 CSV 位置保留。
- [x] 小屏请求在自己端口的 DMA 前台邮箱消费，用当轮快照响应；队列拥塞不阻塞控制循环，不排队陈旧应答；禁用该端口调试输出。
- [ ] 检查 PA9/PA10/PA8 的调功器占用、PA2/PA3/PA4 的联控占用；只采用无主站竞争的分配。保留可控硅硬件时不得直接占用其端口，不能自动把两个 Modbus 主站并到一个端口。
- [x] 更新 `USER/UART.uvprojx` 源清单；60 KiB 程序边界和原有排污禁用条件保持。

## Task 3: 集成回归与交付

- [x] 用真实适配函数验证读屏不改变联控会话/控制权，数据来自本轮输出，拒绝写入，错误请求不能影响保护；测试 DMA 接收的主/从角色及实际发送端口。
- [x] 运行分机 host suites、control/parser、DMA adapter、legacy TX、Flash layout、ARM C compile；必要时运行总仓库跨固件回归。
- [x] 保存原 CSV（不修改来源文件），生成带状态名称的导入 CSV 和寄存器/接线/端序/轮询配置文档；CSV 不包含 HMI 页面布局，说明屏工程需要绑定的三个状态灯。
- [x] 独立规格及对抗式代码审核通过，相关具体反例已修复复核。

交付要求：以 `alex <wmstianya@gmail.com>` 提交分机和总仓库配套分支 `codex/slave-small-screen`，推送并向既有重构分支创建 PR；不合并、不烧录。
