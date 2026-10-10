"""主机侧检查 SD 配置的顺序/键码与本地固件解析约定。"""
from pathlib import Path
import re
from xml.etree import ElementTree as ET

base = Path(__file__).resolve().parents[1]
config = base / "SD-card" / "config.xml"
raw = config.read_text(encoding="utf-8")
root = ET.fromstring(raw)
assert root.tag == "Configuration"
assert [x.tag for x in root] == ["Settings", "Profiles"]
assert [x.tag for x in root.find("Settings")] == [
    "Clicky_P", "Clicky_I", "Twist_P", "Twist_I", "Momentum_P", "Momentum_I"
]
profiles = root.find("Profiles").findall("Profile")
assert [x.get("name") for x in profiles] == ["Mac常用", "Win常用"]
labels = ["复制", "粘贴", "剪切", "全选", "撤销", "退格"]
letters = [67, 86, 88, 65, 90]
for profile, modifier in zip(profiles, [91, 17]):
    assert len(profile.get("name").encode("utf-8")) < 32
    assert [x.tag for x in profile] == [
        "LED_Mode", "LED_Primary", "LED_Secondary", "LED_Brightness",
        "WheelMode", "WheelKey", "MacroButtons"
    ]
    assert profile.findtext("WheelMode") == "Clicky"
    assert profile.findtext("WheelKey") == "0"
    buttons = profile.find("MacroButtons").findall("MacroButton")
    assert len(buttons) == 6
    for i, button in enumerate(buttons):
        assert [x.tag for x in button] == ["Action", "Action", "Action", "Label"]
        assert button.findtext("Label") == labels[i]
        assert len(labels[i].encode("utf-8")) < 32
        actions = [x.text for x in button.findall("Action")]
        expected = [f"0,{modifier}", f"0,{letters[i]}", "0,0"] if i < 5 else ["0,8", "0,0", "0,0"]
        assert actions == expected, (profile.get("name"), i, actions)
        assert all(re.fullmatch(r"\d+,\d+", item) for item in actions)

# 主机静态对应检查：这些 VK 必须是固件认可的修饰键/字母键/Backspace。
keys = (base / "MacroPad" / "keyCodes.ino").read_text()
assert re.search(r"case 17:.*?return KEYBOARD_MODIFIER_LEFTCTRL;", keys, re.S)
assert re.search(r"case 91:.*?return KEYBOARD_MODIFIER_LEFTGUI;", keys, re.S)
assert re.search(r"case 8:.*?return HID_KEY_BACKSPACE;", keys, re.S)
for vk, hid in zip(letters, "CVXAZ"):
    assert re.search(rf"case {vk}:\s*return HID_KEY_{hid};", keys)
print("通过：Mac/Win 两配置、各 6 键、中文标签、顺序与固件 VK 映射")
