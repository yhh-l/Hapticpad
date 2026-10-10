# Hapticpad v2 中文固件（实验版，**未实机验证**）

> **先看硬件状态**：本发布是 `phantomdubs/Hapticpadv2` 的 Arduino 固件派生版，不是本仓库原有 `Software/MacroPad` 的 V1 更新，也不是编码器故障的修复固件。当前测试设备旋转时 **A / GP27 未产生有效数字高电平**，B / GP26 正常切换；在 A 相未修复前无法可靠判断正反转。**不要因为下载了本固件就认为滚动乱跳已解决**，更不要凭软件问题盲目调换电机 U/V/W 相线或修改 FOC。来源快照：[`phantomdubs/Hapticpadv2` commit `566f9ff`](https://github.com/phantomdubs/Hapticpadv2/commit/566f9ff060736618fc0d4b85d26c0c9ab5eb96bb)。原设计与固件归各原作者；沿用仓库根目录的 CERN-OHL-S-2.0 许可证。

**本机诊断依据（2026-10-10）**：ADC 0–4095，慢转约一圈时 A/GP27 为 46–353、跨 1000/3000 阈值的有效边沿 0 次；B/GP26 为 31–4095、有效边沿 2487 次。这是**该台设备**的测量，不代表所有硬件都如此，也不足以单独判定是芯片、磁铁、焊接还是 GPIO 损坏。

## 下载什么

| 文件 | 作用 |
|---|---|
| [`release/HapticpadV2_CN_EXPERIMENTAL.uf2`](release/HapticpadV2_CN_EXPERIMENTAL.uf2) | 本目录源码在主机重新编译的唯一候选 UF2；**尚未刷机** |
| [`SD-card/config.xml`](SD-card/config.xml) | 复制到 MicroSD 根目录的 `/config.xml`，Mac/Windows 两套六键配置；刷 UF2 不会自动写入 SD 卡 |
| [`MacroPad/`](MacroPad/) | Arduino 固件源码；不覆盖原仓库 V1 |
| [`tests/`](tests/) | 主机侧菜单、Clicky 步进及 XML 键位检查 |

**适用目标**：Waveshare RP2040 Plus **16 MB** 无排针版 + 原 Hapticpad v2 硬件接线。其他 Pico/RP2040 板型、闪存规格、编码器接法和电机参数未适配。固件需要 SD 卡中的 `/config.xml`；请不要把 UF2 放到 SD 卡当成刷机。`/config.xml` 也不能解决 `SD.begin()` 初始化失败。

## 做了哪些优化

- 128×64 OLED 中文菜单显示配置名、六个宏键标签和按键详情。Mac 配置的 GUI 组合键提示为 `Cmd+`，Windows 为 `Win+`；未映射的 VK 键码不按标签猜测其作用。
- 第 7 / 8 个小按钮短按松开切换配置，长按约 450 ms 或同时按下进入菜单；菜单步进使用 0.8 格门槛和移动中心，减少边界回弹触发。普通 Clicky 滚动也使用同样的迟滞逻辑；菜单中不向电脑发送滚轮事件。
- 六键示例：Mac 使用 Command、Windows 使用 Ctrl，依次复制、粘贴、剪切、全选、撤销、退格。第 6 键发送 **Backspace**，不是文件管理器的删除命令；Mac Finder 中的“剪切”文件也不等同于 Command+X。请先在普通文本框测试。
- 本次发布额外修正：`<Settings>` 节点缺失时显式关闭已经打开的 SD 配置文件，然后返回失败。
- 保留 v2 基线的电机/编码器引脚、FOC 初始化与电机相序；并非通过改变硬件方向来掩盖 A 相异常。

## 使用与回退

1. 先确认设备的 **A/GP27 与 B/GP26 电平及供电安全**。如果 A 相仍异常、接线可能把 5 V 信号直送 3.3 V GPIO、或电机发热/自转，请停止上电测试，先排查硬件。当前设备的 A 相问题**尚未由本固件排除**。
2. 备份 SD 根目录的 `/config.xml`、`/last.txt` 和原有 BMP 文件夹；另备份现用 UF2，记录对应板型。需要 Mac/Win 示例键位时，才将本目录的 `SD-card/config.xml` 复制到 SD 根目录。不要覆盖无备份的私人配置。
3. 硬件恢复并确认型号后，可在断电状态下按住 RP2040 的 BOOTSEL 接入电脑，在出现的 `RPI-RP2` 盘复制 UF2。**发布者未替你刷写或验证该步骤**。使用新配置时，如旧 `/last.txt` 记录不同的配置序号，请备份后移走旧记录，让固件默认打开第一项 Mac 配置。
4. 首次验证按静置、屏幕/菜单、六键、顺转三格/逆转三格、力反馈的顺序进行。若滚动方向异常、静止跳动、过热或异响，**立即断电**，不要继续调 FOC 或换相线。回退时在 BOOTSEL 模式刷回你备份的旧 UF2，并恢复原 SD 配置与记录。

## 构建与主机检查

主机编译环境：Arduino-Pico core 6.3.0、Waveshare RP2040 Plus / 16 MB 无 FS / TinyUSB；Simple FOC 2.3.5、U8g2 2.35.30、SdFat 2.3.0、FastLED 3.9.15、Adafruit TinyUSB Library 3.7.7。进入**本目录**后运行：

```sh
arduino-cli compile \
  --fqbn 'rp2040:rp2040:waveshare_rp2040_plus:flash=16777216_0,usbstack=tinyusb' \
  --output-dir /tmp/hapticpad-v2-cn-build MacroPad
clang++ -std=c++11 -Wall -Wextra -Werror tests/menu_step_test.cpp -o /tmp/menu-step-test && /tmp/menu-step-test
clang++ -std=c++11 -Wall -Wextra -Werror tests/clicky_detent_test.cpp -o /tmp/clicky-detent-test && /tmp/clicky-detent-test
python3 tests/test_mac_win_config.py
python3 tests/check_uf2.py release/HapticpadV2_CN_EXPERIMENTAL.uf2
```

本机上述主机测试及交叉编译通过；第三方 Simple FOC 在当前工具链有枚举/浮点弃用警告。**编译成功不等于真实设备已刷写、HID 已验证、编码器已恢复或电机力矩安全**。

### 来源和公开范围

保留了 `phantomdubs/Hapticpadv2` 的固件接口与配置格式；本仓库原 V1 源码/UF2 仍在原位。发布目录只包含源码、示例 SD XML、主机测试和单个候选 UF2，不包含现场串口日志、个人 SD 配置或诊断固件。
