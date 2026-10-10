// --- display.ino ---



void loadButtonIcons(){
  char filePath[maxProfileNameLength + 6];

  snprintf(filePath, sizeof(filePath), "/%s/1.bmp", profileName);
  loadBMP24x24(filePath, icon1);

  snprintf(filePath, sizeof(filePath), "/%s/2.bmp", profileName);
  loadBMP24x24(filePath, icon2);

  snprintf(filePath, sizeof(filePath), "/%s/3.bmp", profileName);
  loadBMP24x24(filePath, icon3);

  snprintf(filePath, sizeof(filePath), "/%s/4.bmp", profileName);
  loadBMP24x24(filePath, icon4);

  snprintf(filePath, sizeof(filePath), "/%s/5.bmp", profileName);
  loadBMP24x24(filePath, icon5);

  snprintf(filePath, sizeof(filePath), "/%s/6.bmp", profileName);
  loadBMP24x24(filePath, icon6);
}


// 固定用 GB2312 字库绘制 UTF-8（配置中的常见简体中文及英文）。
void drawClippedUTF8(int x, int y, const char *text, int width) {
  char buf[96] = {0};
  size_t n = 0;
  for (const unsigned char *p = (const unsigned char*)text; *p && n < sizeof(buf) - 4;) {
    size_t bytes = (*p < 0x80) ? 1 : ((*p & 0xe0) == 0xc0 ? 2 : ((*p & 0xf0) == 0xe0 ? 3 : 4));
    bool valid = true;
    for (size_t j = 1; j < bytes; ++j) {
      if (p[j] == 0 || (p[j] & 0xc0) != 0x80) { valid = false; break; }
    }
    if (!valid) { ++p; continue; }
    memcpy(buf + n, p, bytes);
    buf[n + bytes] = '\0';
    if (u8g2.getUTF8Width(buf) > width) { buf[n] = '\0'; break; }
    n += bytes;
    p += bytes;
  }
  u8g2.drawUTF8(x, y, buf);
}

const char* keyName(uint8_t vk, char* buf, size_t size) {
  if (vk >= 65 && vk <= 90) { snprintf(buf, size, "%c", vk); return buf; }
  if (vk >= 48 && vk <= 57) { snprintf(buf, size, "%c", vk); return buf; }
  if (vk >= 112 && vk <= 123) { snprintf(buf, size, "F%d", vk - 111); return buf; }
  if (vk >= 96 && vk <= 105) { snprintf(buf, size, "Num%d", vk - 96); return buf; }
  switch (vk) {
    case 0: return "无";
    case 8: return "退格"; case 9: return "Tab"; case 13: return "Enter";
    case 16: return "Shift"; case 17: return "Ctrl"; case 18: return "Alt";
    case 27: return "Esc"; case 32: return "空格";
    case 33: return "PgUp"; case 34: return "PgDn"; case 35: return "End";
    case 36: return "Home"; case 37: return "左"; case 38: return "上";
    case 39: return "右"; case 40: return "下"; case 45: return "Ins";
    case 46: return "Del"; case 91: return "Win"; case 92: return "Win";
    case 173: return "静音"; case 174: return "音量-"; case 175: return "音量+";
    case 186: return ";"; case 187: return "="; case 188: return ",";
    case 189: return "-"; case 190: return "."; case 191: return "/";
    case 192: return "`"; case 219: return "["; case 220: return "\\";
    case 221: return "]"; case 222: return "'";
  }
  snprintf(buf, size, "VK%u未映射", (unsigned)vk); // 未识别码不擅自猜用途
  return buf;
}

void drawActiveProfile(){
  u8g2.setFont(u8g2_font_wqy12_t_gb2312);
  u8g2.drawUTF8(0, 12, "配置");
  drawClippedUTF8(29, 12, profileName, 71);
  char fraction[16];
  snprintf(fraction, sizeof(fraction), "%d/%d", activeProfile + 1, totalProfiles);
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.drawStr(104, 9, fraction);
  u8g2.drawHLine(0, 14, 128);
  u8g2.setFont(u8g2_font_wqy12_t_gb2312);
  for (int i = 0; i < 6; ++i) {
    int x = (i % 3) * 42;
    int y = 18 + (i / 3) * 19;
    char num[4]; snprintf(num, sizeof(num), "%d", i + 1);
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(x, y + 8, num);
    u8g2.setFont(u8g2_font_wqy12_t_gb2312);
    drawClippedUTF8(x + 9, y + 11, buttonLabel[i][0] ? buttonLabel[i] : "未设", 32);
  }
  u8g2.setFont(u8g2_font_wqy12_t_gb2312);
  u8g2.drawUTF8(0, 64, "7/8短按切换 长按菜单");
}

void drawKeyDetail(int button){
  u8g2.setFont(u8g2_font_wqy12_t_gb2312);
  char title[20]; snprintf(title, sizeof(title), "按键%d  功能", button + 1);
  u8g2.drawUTF8(0, 13, title);
  drawClippedUTF8(0, 27, buttonLabel[button][0] ? buttonLabel[button] : "未命名", 128);
  int modifier = 0, line = 0;
  for (int i = 0; i < 3; ++i) {
    uint8_t vk = macroAction[button][i];
    if (!vk) continue;
    int mod = checkModifiers(vk);
    if (mod) { modifier |= mod; continue; }
    char name[20], combo[72] = {0};
    if (modifier & KEYBOARD_MODIFIER_LEFTCTRL) strcat(combo, "Ctrl+");
    if (modifier & KEYBOARD_MODIFIER_LEFTSHIFT) strcat(combo, "Shift+");
    if (modifier & KEYBOARD_MODIFIER_LEFTALT) strcat(combo, "Alt+");
    // HID GUI modifier is Command on macOS and Windows key on Windows.
    const bool macProfile = strncmp(profileName, "Mac", 3) == 0;
    if (modifier & KEYBOARD_MODIFIER_LEFTGUI) strcat(combo, macProfile ? "Cmd+" : "Win+");
    if (modifier & KEYBOARD_MODIFIER_RIGHTGUI) strcat(combo, macProfile ? "Cmd+" : "Win+");
    strncat(combo, keyName(vk, name, sizeof(name)), sizeof(combo) - strlen(combo) - 1);
    char label[80]; snprintf(label, sizeof(label), "%d: %s", ++line, combo);
    drawClippedUTF8(0, 27 + line * 12, label, 128);
  }
  if (!line) {
    u8g2.drawUTF8(0, 44, modifier ? "仅修饰码，未发送" : "无按键动作");
  }
  if (line < 3) u8g2.drawUTF8(0, 64, "功能由电脑软件决定");
}

void drawProfileMenu(){
  u8g2.setFont(u8g2_font_wqy12_t_gb2312);
  u8g2.drawUTF8(0, 12, "滚动选 按7/8确认");
  char indexText[20]; snprintf(indexText, sizeof(indexText), "%d/%d", activeProfile + 1, totalProfiles);
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.drawStr(97, 10, indexText);
  u8g2.setFont(u8g2_font_wqy12_t_gb2312);
  if (activeProfile > 0) drawClippedUTF8(3, 28, profileNames[activeProfile - 1], 122);
  u8g2.drawBox(0, 29, 128, 17);
  u8g2.setDrawColor(0);
  drawClippedUTF8(3, 43, profileNames[activeProfile], 122);
  u8g2.setDrawColor(1);
  if (activeProfile + 1 < totalProfiles) drawClippedUTF8(3, 60, profileNames[activeProfile + 1], 122);

}
