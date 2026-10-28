# Hapticpad V1-CN-1.9.3-RADIO

本版本修复配置选择与确认流程，保留 1.9.1 的 Win/Mac 数字组合键支持。

## 适用硬件

- RP2040 Plus 16 MB，Hapticpad V1 接线。
- 固件使用 TinyUSB；请先确认硬件与接线相符。

## 本机实体键位与菜单

本版按已确认的实物编号显示提示：实体7接GP14、实体8接GP13。
内部绑定序号及SD数据保持兼容，只修改屏幕提示和说明，没有交换GPIO或快捷键绑定。

1. 长按实体8约0.7秒，出现列表后松开。
2. 旋钮选择配置。
3. 短按并松开实体7，应用并退出；实体8取消。

长按实体7进入设置。设置中短7确认字段、短8返回；长7保存并退出，长8放弃并退出。
切换失败保留列表并提示“切换失败 7重试”。

## Mac普通快捷键/滚动不响应时

Mac中刻度、轻刻度等手感档位共用USB HID输出，不需要电台驱动或CAT控制台。
但每个档位各自保存快捷键、滚动修饰键与方向，手感名称并不能保证绑定完全相同。

1. 先退出配置选择/设置；菜单内会暂停发送普通按键和滚动。
2. 查看Mac是否识别到Hapticpad键盘/鼠标，使用数据线连接；未枚举USB设备时先排查连接。
3. 打开工具箱输入检测区，先单独按实体6（退格），再转旋钮，分别检查按键与滚动。
4. 若退格/滚动正常而复制不正常，检查当前Mac配置是否使用Cmd（VK91）而非Ctrl/Win预设；复制需在有选中文字的应用中验证。
5. 若退格正常、旋钮不动，再核对当前配置WheelMode、WheelKey及编码器状态。
6. HP INFO为只读状态命令：version、sd、disk_mode、settings_open、menu_kind、keyboard_ready、mouse_ready、last_binding_mask用于区分连接、菜单与输出路径。

固件串口HP HUD只更新电台看板，不参与普通Mac按键/滚动。
本次本机未发现设备串口，现场故障原因尚未确认；未刷固件、未改SD。

## Win/Mac 数字组合键

固件支持同时按 4+5+6 输出 813816 后发送一次 Enter。长按只触发一次，松开后可以再次触发。需要 SD 卡 config.xml 中对应的 NumberText/EnterAfterText 组合键配置；仅更新 UF2 不会自动添加这些配置。

源码包附带 tools/Add-Number456.py，可为名称以 Win 或 Mac 开头的配置生成此组合键，输出到单独文件：

```text
python tools/Add-Number456.py config.xml config-number456.xml
```

先备份 SD 卡原 config.xml，检查生成结果后再替换。此脚本会设置 4+5+6 对应的组合键；其他按键配置保持原有内容。

## 刷入

下载 Hapticpad_V1_CN_1.9.3_RADIO.uf2。让对应设备进入 BOOTSEL，出现 RPI-RP2 磁盘后复制 UF2。设备会重启；可用 115200 波特串口发送 HP INFO，核对 version 为 V1-CN-1.9.3-RADIO。

## 验证范围

本版在1.9.2基础上调整实体键位提示，保留原菜单调度、HUD和数字宏。
本地构建/回归结果见build/manifest-1.9.3.json；设备与Windows现场通讯待验收。
没有沿用1.9.2的实机测试作为本版验收结果。

## 源码与构建

源码位于仓库分支 codex/physical-key-labels 的 custom/radio-v1/MacroPad。

构建基于 Arduino-Pico 6.3.1，FQBN：

```text
rp2040:rp2040:waveshare_rp2040_plus:flash=16777216_0,usbstack=tinyusb
```

依赖版本及完整流程见.github/workflows/build-radio.yml；源码包附build-radio.yml。原项目许可证见 LICENSE。
