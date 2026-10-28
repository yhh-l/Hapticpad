# Hapticpad V1-CN-1.9.2-RADIO

本版本修复配置选择与确认流程，保留 1.9.1 的 Win/Mac 数字组合键支持。

## 适用硬件

- RP2040 Plus 16 MB，Hapticpad V1 接线。
- 固件使用 TinyUSB；请先确认硬件与接线相符。

## 配置选择

1. 长按 7 约 0.7 秒，出现配置列表后松开。
2. 旋转旋钮选择配置。
3. 短按并松开 8，应用配置并退出。
4. 短按 7 取消选择。

长按 8 进入设备设置。在设备设置中，短按 8 确认当前编辑字段，长按 8 保存整页设置，长按 7 放弃。

切换失败时保留配置列表并显示“切换失败 8重试”，原配置继续工作。配置解析及电机暂停握手成功后才应用新配置。

## Win/Mac 数字组合键

固件支持同时按 4+5+6 输出 813816 后发送一次 Enter。长按只触发一次，松开后可以再次触发。需要 SD 卡 config.xml 中对应的 NumberText/EnterAfterText 组合键配置；仅更新 UF2 不会自动添加这些配置。

源码包附带 tools/Add-Number456.py，可为名称以 Win 或 Mac 开头的配置生成此组合键，输出到单独文件：

```text
python tools/Add-Number456.py config.xml config-number456.xml
```

先备份 SD 卡原 config.xml，检查生成结果后再替换。此脚本会设置 4+5+6 对应的组合键；其他按键配置保持原有内容。

## 刷入

下载 Hapticpad_V1_CN_1.9.2_RADIO.uf2。让对应设备进入 BOOTSEL，出现 RPI-RP2 磁盘后复制 UF2。设备会重启；可用 115200 波特串口发送 HP INFO，核对 version 为 V1-CN-1.9.2-RADIO。

## 验证范围

- GitHub Actions 构建成功：40615430452。
- 按键处理回归覆盖长7、旋转、短8确认、取消，以及解析失败和电机握手超时后重试。
- 数字宏、配置解析及 HUD 检查通过。
- 实机刷入后已确认版本、SD 卡、USB 键盘/鼠标及电机就绪。
- 此版本的实体菜单按键流程仍待用户验收；Mac 端尚未实测。未进行完整闪存逐字节回读。

## 源码与构建

源码位于仓库分支 codex/profile-menu-confirm-fix 的 custom/radio-v1/MacroPad。

构建基于 Arduino-Pico 6.3.1，FQBN：

```text
rp2040:rp2040:waveshare_rp2040_plus:flash=16777216_0,usbstack=tinyusb
```

依赖版本及完整流程见源码包 build-radio.yml。原项目许可证见 LICENSE。
