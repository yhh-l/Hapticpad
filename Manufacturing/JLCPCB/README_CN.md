# 嘉立创 PCBA 参考文件

本目录的 BOM 和 CPL/坐标文件由当前 KiCad PCB 源文件整理，用于以后选择嘉立创 SMT/PCBA 时参考。本次只下单裸 PCB，**不要上传这些文件**；主板和控制板仍然分别只上传对应的 Gerber ZIP。

## 文件对应关系

| PCB | BOM | CPL/坐标文件 |
|---|---|---|
| 主板 | `MacroPad_BOM.csv` | `MacroPad_CPL.csv` |
| 控制板 | `Controller_BOM.csv` | `Controller_CPL.csv` |

## 已核对内容

- 主板 BOM/CPL 包含 42 个顶层 SMD 位号：`C2-C21`、`D1-D20`、`S1-S2`。
- 控制板 BOM/CPL 只包含顶层 SMD 电容 `C1`；TMC6300、RP2040-Plus、排针和测试点不在贴片文件内，按本项目装配方式后焊。
- `C2-C21`：100nF / 50V / X7R / 0603，对应 [LCSC C14663](https://www.lcsc.com/product-detail/YAGEO-C14663.html)。
- `D1-D20`：Worldsemi `WS2812B-B/W`，SMD5050-4P，对应 [LCSC C114586](https://www.lcsc.com/product-detail/RGB-LEDs-Built-in-IC_Worldsemi-WS2812B-B-W_C114586.html)。
- `C1`：10nF / 50V / X7R / 0603，对应 [LCSC C307346](https://www.lcsc.com/product-detail/C307346.html)。
- `S1-S2`：已按本次采购改为 Panasonic [`EVQ-Q2K03W`](https://www.digikey.com/en/products/detail/panasonic-industry/EVQ-Q2K03W/762893)，6.5×6.0mm 顶按 SMD；当前未填写 LCSC 料号，上传 PCBA 时需要在嘉立创页面手动匹配、客供或设为不贴。
- KiCad 内部仍保留旧库名 `SW_EVQ-Q2B02W`，但元件 Value 已更新为 `EVQ-Q2K03W`。现有焊盘中心距为 6.8×4.0mm，与 K03W 的 J 型引脚间距一致，因此不改铜层和 Gerber。
- CPL 坐标、层和旋转角来自当前 `.kicad_pcb`；所有列出的器件都位于顶层。LED 按环形布局每颗递增 18°。

> [!WARNING]
> BOM/CPL 能被识别不代表方向一定正确。真正下 PCBA 单时，必须在嘉立创贴片预览中逐一检查 `D1-D20` 的 1 脚/DIN-DOUT 方向，并确认 `S1-S2` 外形和焊盘重合。若平台匹配了替代料，也要重新核对封装和方向后再付款。
