# 自动排污定时开阀与 E3 暂停 Implementation Plan

> **For agentic workers:** Use subagent-driven-development for the isolated water-level pause API; root integrates the blowdown state machine and adapter, then obtains independent specification and adversarial reviews. Preserve existing file encodings.

**Goal:** 自动排污先停热，再按固件参数定时开阀（默认10秒）；仅开阀期间暂停 E3 确认，保留采样与停热保护；通过新 PR 交付。

**Architecture:** 在已授权、完成停热检查的自动排污 OPEN 阶段暂停水位组合报警，不因低水位或电极组合波动提前结束定时开阀。失效采样、停机、失联/许可撤销、压力保护、阀反馈丢失和加热输出重新出现仍会关阀。退出 OPEN 后恢复正常判断，清除旧的稳定资格，必须重新采样、补水并稳定后才允许加热。手动排污保留原水位中止行为。

**Tech Stack:** STM32F103C8T6 / C90 production code / native GCC regression / ARM C-object checks / GitHub PR.

## 已确认的范围

- 用户确认只在开阀期间暂停 E3，保留采样和停热，关阀后补水并稳定。
- 开阀时长先作为固件参数，不新增屏端写入或 Flash 保存。
- 默认10秒是请求值，不能代替实际设备工艺验证。现有共管许可、物理阀反馈及持久会话准入保持；当前生产适配仍未具备这些准入，排污不会因此直接启用。
- 新分支 `codex/timed-auto-blowdown` 基于 `codex/slave-small-screen`；分机及总仓库分别新建 PR，不修改既有小屏 PR 的范围。

## Task 1: 独立 E3 暂停 API

Files: `UART/SYSTEM/system_control/water_level_core.c/.h`, `UART/tests/test_water_level_core.c`.

- [x] 先添加失败测试：暂停期间持续非法组合超过确认时限仍采样但不产生新 E3；热许可始终为0；已有报警不被清除；退出后要求新采样及完整稳定时段；非法组合确认重新起算；失效采样和计时回绕仍正确。
- [x] 增加 `alarm_paused` 状态与 `water_level_set_alarm_pause(state, paused, now_ms)`。切换时清除 `invalid_active` 与恢复资格，不清除 `logic_alarm`。重复设置不重启普通计时。
- [x] 暂停期间 sample 更新原始水位和采样时间，service 不累积非法时长、不授予热许可；退出后只有新样本能重启恢复/异常计时。
- [x] 运行 `python scripts/run_host_tests.py --test water_level_core`。

## Task 2: 自动排污策略与固件时长

Files: `UART/SYSTEM/system_control/blowdown_core.c/.h`, `UART/SYSTEM/system_control/slave_water_control.c/.h`, `UART/tests/test_blowdown_core.c`, `UART/tests/test_slave_water_control.c`.

- [x] 固件宏 `WATER_BLOWDOWN_OPEN_SECONDS` 默认10，可显式覆盖1～60秒；编译期拒绝越界。该范围仅防止配置错误，不代表全部时长均适合现场。
- [x] 原 `blowdown_request` 保持手动策略；新增 `blowdown_request_timed` 在成功接纳请求时固定自动策略。自动来源0/2调用新入口，手动来源1/3保留原入口及默认1.5秒，排队中的其他请求不能更改策略或时长。
- [x] 只有自动 OPEN 阶段放宽水位 mask 条件，保留采样新鲜度及所有非水位保护。预备、关阀、补水和手动流程仍使用原判断。
- [x] 适配器在采样/服务/读取热许可前同步 E3 暂停状态；进入与退出 OPEN 后再次同步，使旧稳定水位不能直接恢复加热。
- [x] 最终开阀检查增加开阀时限检查，即使状态服务尚未运行，超过设定时间也不能再次发出开阀命令。
- [x] 测试10秒边界、编译覆盖、低/非法水位、停止/许可/压力/采样失效/反馈/热输出中断、计时回绕、退出后 E3 与稳定恢复、手动策略以及事件原始水位记录。

## Task 3: 集成验证及交付

Files: `UART/docs/water-blowdown-stability.md`, root `TIMED_BLOWDOWN_DELIVERY.md`, root `WATER_STABILITY_IMPLEMENTATION.md`, submodule pointer.

- [x] 运行 host suites、生产控制/解析集成、主从排污协议回归、默认 ARM C 编译和参数覆盖/非法值检查。
- [x] 核对联控仍按原 B1 协议互锁，开阀/恢复中不重新计入可用热容量；不新增未经验证的物理反馈或会话。
- [x] 独立规格审核及后续对抗式代码审核均通过，无剩余阻断项；修正文档中水位资格与实际闭阀等待可重叠的时序表述。
- 交付步骤：使用 `alex <wmstianya@gmail.com>` 提交分机及总仓库，推送并通过 GitHub 插件创建两个关联 PR；以最终远端 SHA 与 PR 元数据核验交付。记录完整链接、实板时序及工艺尚未验证。
