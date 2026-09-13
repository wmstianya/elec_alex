# 电蒸汽联控系统 — 总仓库

本仓库是电蒸汽「联控 + 分机」的总入口(umbrella repo),**本身不存放代码**,只通过 git submodule 锁定两个子项目的一组匹配版本,用于成套发布、备份与换机。

| 目录 | 角色 | 子仓库 | 器件(Keil 工程) |
|------|------|--------|------------------|
| `UART-elec/` | **联控**主控固件 | https://github.com/wmstianya/elec_steam | STM32F103VE |
| `UART/` | **分机**固件 | https://github.com/wmstianya/elec_steam_medium | STM32F103C8 |

通讯拓扑:联控通过 USART4(9600,Modbus RTU)轮询 1~10 号分机;分机 USART2 接屏(地址 200)或联控(拨码 1~15)。

## 克隆

```bash
git clone --recursive https://github.com/wmstianya/elec_alex.git
```

已克隆但子目录为空时:

```bash
git submodule update --init --recursive
```

## 日常使用

```bash
# 1) 改代码:进子目录,与普通仓库一样
cd UART-elec
git add -A && git commit -m "..." && git push

# 2) 回总仓库,把指针更新到子项目的新提交
cd ..
git add UART-elec
git commit -m "bump UART-elec"
git push

# 3) 拉取总仓库的更新(含子项目指针)
git pull
git submodule update --init --recursive
```

## 注意

- submodule 是**版本指针**,不是内容同步:不执行第 2 步,总仓库仍锁定旧提交(状态里会提示 `new commits`,不影响子目录使用)。
- 推送上**先推子仓库、再推总仓库**,否则别人 `git submodule update` 取不到子模块的新提交。
- 当前锁定版本:`git submodule status`。
