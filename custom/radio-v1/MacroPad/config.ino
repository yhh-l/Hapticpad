String configXML;
String textTag(const String &block, const char *tag, const String &fallback = "") {
  String start = String("<") + tag + ">", end = String("</") + tag + ">";
  int a = block.indexOf(start);
  if (a < 0) return fallback;
  a += start.length(); int b = block.indexOf(end, a);
  return b < 0 ? fallback : block.substring(a, b);
}
String xmlDecode(String value) {
  value.replace("&quot;", "\""); value.replace("&apos;", "'");
  value.replace("&lt;", "<"); value.replace("&gt;", ">"); value.replace("&amp;", "&");
  return value;
}
void copyText(char *out, const String &value) {
  unsigned length = min((unsigned)value.length(), 31u);
  if (length < value.length())
    while (length && ((uint8_t)value[length] & 0xc0) == 0x80) --length;
  memcpy(out, value.c_str(), length); out[length] = 0;
}
int parseLED(const String &value) {
  if (value.equalsIgnoreCase("Halo")) return 0;
  if (value.equalsIgnoreCase("Breath")) return 1;
  if (value.equalsIgnoreCase("Bands")) return 2;
  if (value.equalsIgnoreCase("Rainbow")) return 3;
  if (value.equalsIgnoreCase("Solid")) return 4;
  if (value.equalsIgnoreCase("Knob")) return 6;
  if (value.equalsIgnoreCase("KnobTrail")) return 7;
  return 5;
}
void colorValue(const String &text, uint8_t color[3]) {
  unsigned r = 0, g = 0, b = 0;
  if (sscanf(text.c_str(), "%u,%u,%u", &r, &g, &b) == 3) {
    color[0] = min(r, 255u); color[1] = min(g, 255u); color[2] = min(b, 255u);
  }
}
String profileBlock(int index) {
  int position = configXML.indexOf("<Profiles>");
  if (position < 0) return "";
  for (int i = 0; i <= index; ++i) {
    position = configXML.indexOf("<Profile ", position + 1);
    if (position < 0) return "";
  }
  int end = configXML.indexOf("</Profile>", position);
  return end < 0 ? "" : configXML.substring(position, end + 10);
}
bool readActions(const String &item, uint8_t *actions, uint16_t *delays) {
  int cursor = 0;
  for (int action = 0; action < 3; ++action) {
    int a = item.indexOf("<Action>", cursor), b = item.indexOf("</Action>", a);
    if (a < 0 || b < a) return false;
    String pair = item.substring(a+8,b);
    int comma = pair.indexOf(',');
    if (comma < 0) return false;
    delays[action] = constrain(pair.substring(0,comma).toInt(), 0, 2000);
    actions[action] = constrain(pair.substring(comma+1).toInt(), 0, 255);
    cursor = b+9;
  }
  return true;
}
bool readProfile(int index, Profile &output) {
  String block = profileBlock(index);
  int name = block.indexOf("name=\"");
  if (name < 0) return false;
  name += 6; int nameEnd = block.indexOf('"', name);
  if (nameEnd < 0) return false;
  std::unique_ptr<Profile> parsed(new (std::nothrow) Profile);
  if (!parsed) return false;
  Profile &next = *parsed;
  copyText(next.name, xmlDecode(block.substring(name, nameEnd)));
  copyText(next.displayText, xmlDecode(textTag(block, "DisplayText")));
  String scene = textTag(block, "DisplayScene", "Dog");
  next.displayScene = scene == "Text" ? 0 : scene == "Person" ? 2 : scene == "Off" ? 3 : 1;
  String global = textTag(configXML, "Settings");
  next.ledMode = parseLED(textTag(block, "LED_Mode", textTag(global, "LED_Mode", "Solid")));
  next.brightness = textTag(block, "LED_Brightness", "35").toInt();
  next.brightness = constrain(next.brightness, 0, 255);
  next.ledDirection = textTag(block, "LED_Direction", "1").toInt() == -1 ? -1 : 1;
  next.ledOffset = constrain(textTag(block, "LED_Offset", "0").toInt(), 0, 19);
  colorValue(textTag(block, "LED_Primary", textTag(global, "LED_Primary", "0,48,120")), next.primary);
  colorValue(textTag(block, "LED_Secondary", textTag(global, "LED_Secondary", "0,0,0")), next.secondary);
  String mode = textTag(block, "WheelMode", "Clicky");
  next.mode = mode == "Twist" ? 1 : mode == "Momentum" ? 2 : mode == "Flywheel" ? 3 : 0;
  next.wheelKey = textTag(block, "WheelKey", "0").toInt();
  next.direction = textTag(block, "WheelDirection", "1").toInt() == -1 ? -1 : 1;
  String haptic = textTag(block, "HapticMode", "Follow");
  next.feedback.mode = haptic == "Damping" ? DAMPING : haptic == "Assist" ? ASSIST : haptic == "Free" ? FREE : FOLLOW;
  if (next.feedback.mode != FOLLOW) next.mode = 0;
  auto integerTag=[&](const char *tag,long fallback,long lo,long hi){return constrain(textTag(block,tag,String(fallback)).toInt(),lo,hi);};
  next.flywheel.durationMs=integerTag("FlywheelDurationMS",20000,3000,30000);
  next.flywheel.halfLifeMs=integerTag("FlywheelHalfLifeMS",6000,1000,12000);
  next.flywheel.triggerRPM=integerTag("FlywheelTriggerRPM",8,3,40);
  next.flywheel.maxRPM=integerTag("FlywheelMaxRPM",120,30,480);
  next.flywheel.boostPercent=integerTag("FlywheelBoostPercent",100,100,180);
  next.flywheel.brakePercent=integerTag("FlywheelBrakePercent",55,30,85);
  next.flywheel.brakeMs=integerTag("FlywheelBrakeMS",80,30,200);
  next.flywheel.captureMs=integerTag("FlywheelCaptureMS",40,20,120);
  float flyVoltage=textTag(block,"FlywheelVoltage","0.8").toFloat();
  next.flywheel.voltage=std::isfinite(flyVoltage)?constrain(flyVoltage,0.f,.9f):0;
  if(next.flywheel.triggerRPM>=next.flywheel.maxRPM)return false;
  next.feedback.strength = textTag(block, "HapticStrength", "25").toInt();
  next.feedback.strength = constrain(next.feedback.strength, 0, 100);
  next.feedback.damping = textTag(block, "HapticDamping", "6").toInt();
  next.feedback.damping = constrain(next.feedback.damping, 0, 100);
  next.feedback.assist = textTag(block, "HapticAssist", "20").toInt();
  next.feedback.assist = constrain(next.feedback.assist, 0, 100);
  next.feedback.detents = textTag(block, "HapticDetents", "40").toInt();
  next.feedback.detents = constrain(next.feedback.detents, 12, 120);
  next.feedback.speedRPM = textTag(block, "HapticSpeedRPM", "60").toInt();
  next.feedback.speedRPM = constrain(next.feedback.speedRPM, 10, 120);
  next.feedback.assistMs = textTag(block, "HapticAssistMs", "250").toInt();
  next.feedback.assistMs = constrain(next.feedback.assistMs, 100, 600);
  next.feedback.triggerRPM = textTag(block, "HapticTriggerRPM", "8").toInt();
  next.feedback.triggerRPM = constrain(next.feedback.triggerRPM, 3, 30);
  float voltage = textTag(block, "HapticVoltage", "0.8").toFloat();
  next.feedback.voltage = std::isfinite(voltage) ? constrain(voltage, 0.f, 3.f) : 0;
  if (next.feedback.triggerRPM >= next.feedback.speedRPM) next.feedback.triggerRPM = next.feedback.speedRPM / 2;
  next.auxiliaryMode = textTag(block, "AuxiliaryMode", "legacy") == "all_custom" ? 2 :
                       textTag(block, "AuxiliaryMode", "legacy") == "custom" ? 1 : 0;
  next.chordWindow = constrain(textTag(block, "ChordWindow", "100").toInt(), 50, 250);
  String buttons = textTag(block, "MacroButtons");
  int cursor = 0;
  for (int button = 0; button < 6; ++button) {
    int start = buttons.indexOf("<MacroButton>", cursor);
    int end = buttons.indexOf("</MacroButton>", start);
    if (start < 0 || end < start) return false;
    String item = buttons.substring(start, end);
    copyText(next.labels[button], xmlDecode(textTag(item, "Label", "未设置")));
    if (!readActions(item, next.actions[button], next.delays[button])) return false;
    cursor = end + 14;
  }
  String auxiliary = textTag(block, "AuxiliaryButtons");
  cursor = 0;
  for (int i = 0; i < 2; ++i) {
    int start = auxiliary.indexOf("<MacroButton>", cursor);
    int end = auxiliary.indexOf("</MacroButton>", start);
    if (start < 0 && !auxiliary.length()) {
      copyText(next.labels[6+i], i ? "上一配置" : "下一配置"); continue;
    }
    if (start < 0 || end < start) return false;
    String item = auxiliary.substring(start, end);
    copyText(next.labels[6+i], xmlDecode(textTag(item, "Label", "未设置")));
    if (!readActions(item, next.actions[6+i], next.delays[6+i])) return false;
    cursor = end + 14;
  }
  String chords = textTag(block, "Chords");
  bool used[256] = {};
  cursor = 0;
  while (true) {
    int start = chords.indexOf("<Chord ", cursor);
    if (start < 0) break;
    int end = chords.indexOf("</Chord>", start);
    if (end < start || next.chordCount >= 247) return false;
    String item = chords.substring(start, end);
    int attr = item.indexOf("mask=\"");
    if (attr < 0) return false;
    int attrEnd = item.indexOf('"', attr + 6);
    if (attrEnd < 0) return false;
    int mask = item.substring(attr+6, attrEnd).toInt();
    if (mask < 1 || mask > 255 || !(mask & (mask-1)) || used[mask]) return false;
    used[mask] = true;
    ChordBinding &binding = next.chords[next.chordCount++];
    binding.mask = mask;
    copyText(binding.label, xmlDecode(textTag(item, "Label", "未设置")));
    if (!readActions(item, binding.actions, binding.delays)) return false;
    String numberText = textTag(item, "NumberText");
    if (item.indexOf("<NumberText>") >= 0) {
      if (!numberText.length() || numberText.length() >= sizeof(binding.numberText)) return false;
      for (unsigned n=0;n<numberText.length();++n) if(numberText[n]<'0' || numberText[n]>'9') return false;
      memcpy(binding.numberText,numberText.c_str(),numberText.length()+1);
      String enter = textTag(item,"EnterAfterText","0");
      if (enter!="0" && enter!="1")return false;
      binding.enterAfterText=enter=="1";
    }
    cursor = end + 8;
  }
  output = next;
  return true;
}
void setLEDProfile() {
  ledMode = profile.ledMode; ledBrightness = profile.brightness;
  memcpy(primaryColour, profile.primary, 3); memcpy(secondaryColour, profile.secondary, 3);
  calculateColourMultiplier();
}
void loadConfiguration() {
  sdDetected = false; profileCount = 0;
  if (!SD.begin(5, SD_SCK_MHZ(12))) return;
  if (!SD.exists("/config.xml") && SD.exists("/config.menu.bak")) SD.rename("/config.menu.bak", "/config.xml");
  FsFile file = SD.open("/config.xml");
  if (!file || file.size() > 65536) { if (file) file.close(); return; }
  configXML = ""; configXML.reserve(file.size() + 1);
  while (file.available()) configXML += (char)file.read();
  file.close();
  for (unsigned i = 0; i < MAX_PROFILES; ++i) {
    if (!readProfile(i, profile)) break;
    memcpy(profileNames[i], profile.name, 32); ++profileCount;
  }
  if (!profileCount) return;
  String settings = textTag(configXML, "Settings");
  Clicky_P = textTag(settings, "Clicky_P", "0.5").toFloat();
  Clicky_I = textTag(settings, "Clicky_I", "0").toFloat();
  Twist_P = textTag(settings, "Twist_P", "0.65").toFloat();
  Twist_I = textTag(settings, "Twist_I", "0.2").toFloat();
  Momentum_P = textTag(settings, "Momentum_P", "0.3").toFloat();
  Momentum_I = textTag(settings, "Momentum_I", "0").toFloat();
  // First migration starts without motor torque. The web control can enable it.
  wantMotor.store(textTag(settings, "MotorEnabled", "0").toInt() == 1);
  FsFile last = SD.open("/last.txt");
  if (!last) last = SD.open("/lastProfile");
  int index = last ? last.parseInt() : 0;
  if (last) last.close();
  activeProfile = constrain(index, 0, profileCount - 1);
  sdDetected = readProfile(activeProfile, profile);
  if (sdDetected) setLEDProfile();
}
