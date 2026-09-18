# Hapticpad 中文复刻说明

> [!IMPORTANT]
> 本仓库是对 [dmcke5/Hapticpad](https://github.com/dmcke5/Hapticpad) 的复刻与国内物料本地化整理。原始机械、PCB 和软件设计归原作者 **dmcke5 / CNCDan** 所有，原项目采用 `CERN-OHL-S-2.0` 许可证。本文档只记录本次复刻的采购选择、已知勘误和 3D 打印注意事项，不声称为原创项目。

- [English README](README.md)
- [原始项目](https://github.com/dmcke5/Hapticpad)
- [原作者演示视频](https://youtu.be/bNUKRJQjuvQ)

## 这次复刻的主要变化

- 磁编码器使用 **MT6701** 模块，必须确认支持 `ABZ / 1024 线`；不使用 AS5600 直接替代。
- 主控使用 **Waveshare RP2040-Plus 16MB 无排针版**。
- 菜单轻触开关实际采购 **EVQ-Q2K03W ×2**；原 BOM 是 EVQ-Q2B03W，装配前要再核对高度和机械干涉。
- RGB 灯珠改为 **WS2812B 5050 / PLCC4**；`2020` 封装与 PCB 焊盘不匹配。
- 控制板 C1 使用 **10nF / 50V / 0603 / 103**，与灯珠旁的 20 颗 100nF / 0603 去耦电容不是同一种规格。
- 原 README 的 `4× M3×6 CSK` 是误写，应为 **4× M2×6 沉头螺丝**。
- 增加原 BOM 遗漏的编码器固定螺丝：建议准备 **M2×4/6/8 自攻螺丝小套装**，实装选合适长度。

## 国内采购参考

> [!NOTE]
> 下图是 **2026-09-18** 的淘宝/天猫购物车快照，不是永久商品链接，价格、店铺和库存都可能变化。**橙色勾选**的是本次选用项，灰色未勾选的是备选或不需要重复购买的项目。下单时以下表规格为准，不要只看商品标题。

![Hapticpad 国内物料购物车参考](domestic-bom-cart.jpg)

### 核心物料

| 物料 | 数量 | 国内搜索词 / 规格 | 重点核对 |
|---|---:|---|---|
| Waveshare 2.42 英寸 OLED | 1 | `2.42inch OLED Module 白色 SSD1309 128x64 7Pin` | 默认 4 线 SPI，模块尺寸需与外壳匹配 |
| MT6701 磁编码器 | 1 | `MT6701 ABZ 1024线 14bit 磁编码器` | 必须确认模块实际引出 A/B 相，并配编码器专用径向/直径充磁磁钢 |
| Mitoot 2804 100KV 云台无刷电机 | 1 | `Mitoot 2804 100KV 中空轴 云台电机` | 必须是中空轴、三相线，外形尺寸对照模型 |
| SparkFun ROB-21867 / TMC6300 | 1 | `ROB-21867 SparkFun TMC6300 三相无刷驱动` | 核对为 ROB-21867 且排针布局相同 |
| RP2040-Plus | 1 | `Waveshare RP2040-Plus 16MB 无排针` | 不要选已焊排针的 `-M` 版 |
| Kailh Choc V1 矮轴 | 需 6，建议买 10 | `Kailh 1350 Choc V1 矮轴 PG1350` | 不要买 Choc V2 或 MX 脚位 |
| Panasonic EVQ-Q2K03W | 2 | `EVQ-Q2K03W 6x6x3.1` | 与原版 B03W 型号不同，装配前核对按钮高度 |
| Mini SD / Micro SD SPI 模块 | 1 | `Mini SD卡模块 SPI 18x18mm` | 只需 1 个；TF 卡已有，无需重复购买 |
| 6×1mm 圆磁铁 | 4，建议买 10+ | `6x1mm 强磁 圆片` | 只用于滚轮磁吸耦合，不是 MT6701 的编码磁钢 |
| WS2812B | 需 20，建议买 25+ | `WS2812B 5050RGB 4脚 PLCC4` | 不要买 2020 封装 |
| LED 去耦电容 | 需 20，可买 50/100 | `100nF 25V 0603 104 X7R` | 每颗 WS2812B 一颗 |
| 控制板 C1 | 1，建议买 5–10 | `10nF 50V 0603 103 X7R` | 不要用 0805/1206 硬压焊盘 |

### 线材、胶水和五金

| 物料 | 建议 |
|---|---|
| 软硅胶线 | 28AWG 多色线，便于区分电源和信号 |
| 热缩管 | 多规格组合包，用于线束绝缘 |
| AB 环氧胶 | 少量固定磁铁和松动部件；不要流入轴承、按键或电机 |
| M3×5×5 热熔铜螺母 | 9 个，建议买 20/50 个备用 |
| M3×6 内六角圆柱头 | 3 个 |
| M3×10 内六角圆柱头 | 2 个 |
| M2.5×4 内六角圆柱头 | 10 个 |
| M2×6 沉头 | 4 个；这是对原 README 的勘误 |
| 编码器固定螺丝 | 建议买 M2×4/6/8 自攻螺丝套装，实装再选长度 |

## 3D 打印说明

打印 `3D Files/STL's` 中的所有文件，其中：

- `Custom Keycap.STL` ×6
- `Menu Button Printed.STL` ×2
- `PCB Spacer.STL` ×2
- 其余 STL 各 ×1

> [!WARNING]
> 上传主体外壳 `Macro Pad - Printed Version.STL` 时，平台可能提示“薄壁/结构过薄”。告警位置是外壳内部的 **LED 灯珠支撑件**，不是普通可忽略提示。建议先让打印厂人工审核；如被拒单或需要更高强度，应在模型中加厚支撑件。若按原模型打印，收货后必须检查是否断裂，安装灯珠时不要对支撑位施加侧向力。

## 安装前必做检查

1. 确认 MT6701 模块真正支持 ABZ 输出及 1024 PPR，不要只看芯片型号。
2. 确认 WS2812B 是 5050 / PLCC4，并核对 1 脚方向。
3. 确认 RP2040-Plus 是 16MB 无排针版，USB 口安装后朝向外侧。
4. 确认 EVQ-Q2K03W 高度不会导致菜单按键顶死或无法触发。
5. 首次上电先不接电机，分阶段测试 USB、3.3V/5V、OLED、SD 卡、TMC6300，最后再接电机。

## 已知勘误与参考

- [原项目 README](https://github.com/dmcke5/Hapticpad)
- [Issue #3：M2×6 沉头螺丝被误写为 M3×6](https://github.com/dmcke5/Hapticpad/issues/3)
- [Issue #4：编码器固定螺丝在 BOM 中遗漏](https://github.com/dmcke5/Hapticpad/issues/4)
- [Issue #12：显示屏 / SD 供电问题讨论](https://github.com/dmcke5/Hapticpad/issues/12)

