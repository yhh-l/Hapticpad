// Chinese derivative of yhh-l/Hapticpad Software/MacroPad (v1).
// Original wiring, six-key layout, three wheel modes, and LED effects retained.
#include <Arduino.h>
#include <U8g2lib.h>
#include <SimpleFOC.h>
#include <Adafruit_TinyUSB.h>
#include <SPI.h>
#include <SdFat.h>
#include <FastLED.h>
#include <hardware/watchdog.h>
#include <pico/bootrom.h>
#include "device.h"
#include "radio_hud.h"
#include "number_macro.h"
#include "neopixel_transport.h"
#include "feiyu_store.h"

// SD validation and FAT calls nest beyond the SDK's default core1 stack.
// Arduino-Pico allocates a separate 8KiB stack before starting setup1.
bool core1_separate_stack = true;
extern int minimumStorageStackFree;
extern ConfigIndex configIndex;
extern uint32_t screenFrameMs, petQuoteId, petQuoteCount;
SdFat SD;
U8G2_SSD1309_128X64_NONAME0_F_4W_SW_SPI u8g2(U8G2_R0, 28, 22, 6, 7, 8);
BLDCMotor motor(7);
BLDCDriver6PWM driver(20, 21, 18, 19, 16, 17);
Encoder encoder(27, 26, 1024);
void doA() { encoder.handleA(); }
void doB() { encoder.handleB(); }
// Physical labels on this enclosure: 7 = GP14, 8 = GP13.
// Internal binding indices remain unchanged for SD compatibility.
constexpr uint8_t buttonPins[8] = {9, 1, 0, 12, 11, 10, 13, 14};
constexpr int NUM_LEDS = 20, DATA_PIN = 15;
CRGB leds[NUM_LEDS];
NeoPixelTransport<NUM_LEDS, DATA_PIN> ledTransport;
byte primaryColour[3] = {0, 48, 120}, secondaryColour[3] = {};
int ledBrightness = 35, ledSpeed = 50, sequenceStep = 0, ledMode = 4;
float redScale = 0, greenScale = 0, blueScale = 0;
bool breathIncrease = true, evenNumber = false;
int haloCount = 0, loopCounter = 0;
Profile profile;
char profileNames[MAX_PROFILES][32] = {};
int profileCount = 0, activeProfile = 0, menuSelection = 0;
bool sdDetected = false;
std::atomic<bool> setupStarted{false}, configReady{false}, diskMode{false};
std::atomic<bool> configBusy{false}, motorPaused{true}, wantMotor{false};
std::atomic<bool> configAcknowledged{false};
std::atomic<bool> focReady{false}, menuOpen{false};
std::atomic<int> wheelEvents{0};
std::atomic<unsigned> generation{0};
std::atomic<float> flywheelMeasuredRPM{0}, flywheelPeakRPM{0};
std::atomic<bool> flywheelArmed{false};
std::atomic<float> rawTelemetry{0}, shaftTelemetry{0}, targetTelemetry{0}, outputTelemetry{0};
std::atomic<int> sensorSign{1};
std::atomic<unsigned> flywheelState{0},flywheelSamples{0};
std::atomic<bool> flywheelLoadStop{false};
std::atomic<uint32_t> flywheelRemaining{0};
float Clicky_P = .5f, Clicky_I = 0, Twist_P = .65f, Twist_I = .2f;
float Momentum_P = .3f, Momentum_I = 0;
extern uint32_t ledFrames;
extern int ledPixel;
uint32_t buttonsMask = 0;
IdleTimer idleTimer;
std::atomic<bool> deviceStandby{false};
void setDeviceStandby(bool asleep) {
  if (deviceStandby.load()==asleep) return;
  deviceStandby.store(asleep);
  wheelEvents.store(0);
  generation.fetch_add(1);
  if (asleep) {
    releaseKeys(); offLED(); u8g2.setPowerSave(1);
  } else {
    u8g2.setPowerSave(0);
  }
}

bool macroActive = false, macroRelease = false;
NumberMacro numberMacro;
int macroStage = 0;
uint8_t macroActions[3] = {};
uint16_t macroDelays[3] = {};
ChordRecognizer chordRecognizer;
uint8_t lastBindingMask = 0;
char lastBindingLabel[32] = {};
uint32_t lastBindingAt = 0;
SettingsMenu settingsMenu;
RadioHUD radioHUD;
uint8_t menuKind = 0; // 1 profile chooser, 2 settings.
uint8_t macroModifier = 0;
uint32_t macroDue = 0;

uint8_t const keyboardDesc[] = {TUD_HID_REPORT_DESC_KEYBOARD()};
uint8_t const mouseDesc[] = {TUD_HID_REPORT_DESC_MOUSE()};
Adafruit_USBD_HID usb_keyboard(keyboardDesc, sizeof(keyboardDesc), HID_ITF_PROTOCOL_KEYBOARD, 2, false);
Adafruit_USBD_HID usb_mouse(mouseDesc, sizeof(mouseDesc), HID_ITF_PROTOCOL_MOUSE, 2, false);
Adafruit_USBD_MSC usb_msc;

void spiSetup() { SPI.setRX(4); SPI.setTX(3); SPI.setSCK(2); }
void message(const char *first, const char *second) {
  u8g2.clearBuffer();
  {
    u8g2.setFont(u8g2_font_wqy12_t_gb2312);
    u8g2.drawUTF8(0, 22, first);
    u8g2.drawUTF8(0, 44, second);
  }
  u8g2.sendBuffer();
}
int32_t mscRead(uint32_t lba, void *buffer, uint32_t length) {
  return SD.card()->readSectors(lba, static_cast<uint8_t *>(buffer), length / 512) ? (int32_t)length : -1;
}
int32_t mscWrite(uint32_t lba, uint8_t *buffer, uint32_t length) {
  return SD.card()->writeSectors(lba, buffer, length / 512) ? (int32_t)length : -1;
}
void mscFlush() { SD.card()->syncDevice(); }

void setup() {
  Serial.begin(115200);
  // Arduino-Pico starts TinyUSB before setup(). Rebuild the complete USB
  // configuration while detached, on the USB-owning core, then re-enumerate.
  TinyUSBDevice.detach();
  pinMode(11, INPUT_PULLUP); pinMode(9, INPUT_PULLUP);
  bool requestedDisk = watchdog_hw->scratch[0] == SD_BOOT_MAGIC;
  watchdog_hw->scratch[0] = 0;
  delay(40);
  diskMode.store(requestedDisk || digitalRead(11) == LOW);
  if (diskMode.load()) {
    spiSetup(); u8g2.begin();
    if (SD.begin(5, SD_SCK_MHZ(12))) {
      sdDetected = true;
      usb_msc.setID("Pico", "Hapticpad SD", "1.0");
      usb_msc.setReadWriteCallback(mscRead, mscWrite, mscFlush);
      usb_msc.setCapacity(SD.card()->sectorCount(), 512);
      usb_msc.setUnitReady(true); usb_msc.begin();
      message("U盘配置模式", "请在网页读取或写入");
    } else {
      message("存储卡挂载失败", "检查存储卡和接线");
    }
    delay(200); TinyUSBDevice.attach();
    setupStarted.store(true);
    return;
  }
  usb_keyboard.begin(); usb_mouse.begin();
  delay(200); TinyUSBDevice.attach();
  encoder.init(); encoder.enableInterrupts(doA, doB);
  motor.linkSensor(&encoder);
  driver.voltage_power_supply = 5; driver.voltage_limit = 5; driver.init();
  motor.linkDriver(&driver);
  motor.voltage_sensor_align = 3; motor.voltage_limit = 3;
  motor.PID_velocity.D = 0; motor.PID_velocity.output_ramp = 1000;
  motor.LPF_velocity.Tf = .025f; motor.P_angle.P = 20; motor.velocity_limit = 4;
  motor.init(); motor.disable();
  setupStarted.store(true);
  while (!configReady.load()) delay(1);
}

void setup1() {
  while (!setupStarted.load()) delay(1);
  if (diskMode.load()) return;
  FastLED.addLeds(&ledTransport, leds, NUM_LEDS);
  FastLED.setDither(0); // Deterministic latched RGB, also at startup.
  for (uint8_t pin : buttonPins) pinMode(pin, INPUT_PULLUP);
  spiSetup(); u8g2.begin();
  loadConfiguration();
  setupFeiyu();
  configReady.store(true);
}

void releaseKeys() {
  if (usb_keyboard.ready()) usb_keyboard.keyboardRelease(0);
  macroActive = macroRelease = false;
  numberMacro.cancel();
  macroModifier = 0;
}

void beginMacro(const uint8_t *codes, const uint16_t *delays, const char *label, uint8_t mask) {
  if (macroActive || !usb_keyboard.ready()) return;
  memcpy(macroActions, codes, sizeof(macroActions));
  memcpy(macroDelays, delays, sizeof(macroDelays));
  macroStage = 0; macroModifier = 0;
  macroRelease = false; macroActive = true;
  macroDue = millis() + macroDelays[0];
  lastBindingMask = mask; copyText(lastBindingLabel, String(label)); lastBindingAt = millis();
}
void beginNumberMacro(const ChordBinding &binding) {
  if (macroActive || !usb_keyboard.ready()) return;
  if(!numberMacro.begin(binding.numberText,binding.enterAfterText,millis()))return;
  macroActive=true; macroRelease=false; macroModifier=0;
  macroDue=millis();
  lastBindingMask=binding.mask; copyText(lastBindingLabel,String(binding.label));lastBindingAt=millis();
}
void startMacro(int index) {
  beginMacro(profile.actions[index], profile.delays[index], profile.labels[index], 1u << index);
}
void startChord(uint8_t mask) {
  if (!mask) return;
  if (!(mask & (mask-1))) {
    int index = 0; while ((1u << index) != mask) ++index;
    startMacro(index); return;
  }
  for (unsigned i = 0; i < profile.chordCount; ++i) {
    const ChordBinding &binding = profile.chords[i];
    if (binding.mask == mask) {
      if(binding.numberText[0])beginNumberMacro(binding);
      else beginMacro(binding.actions, binding.delays, binding.label, mask);
      return;
    }
  }
}
void tickMacro() {
  if (macroActive && numberMacro.active()) {
    if(!usb_keyboard.ready())return;
    uint8_t vk=0;
    int event=numberMacro.tick(millis(),vk);
    if(event==1){uint8_t keys[6]={(uint8_t)convertKeycode(vk),0,0,0,0,0};usb_keyboard.keyboardReport(0,0,keys);}
    else if(event==2)usb_keyboard.keyboardRelease(0);
    if(!numberMacro.active()){macroActive=false;macroRelease=false;macroModifier=0;}
    return;
  }
  if (!macroActive || (int32_t)(millis() - macroDue) < 0 || !usb_keyboard.ready()) return;
  if (macroRelease || macroStage >= 3) { releaseKeys(); return; }
  while (macroStage < 3) {
    uint8_t vk = macroActions[macroStage];
    int mod = checkModifiers(vk);
    if (mod) macroModifier |= mod;
    else if (vk) {
      uint8_t keys[6] = {(uint8_t)convertKeycode(vk), 0, 0, 0, 0, 0};
      usb_keyboard.keyboardReport(0, macroModifier, keys);
    }
    ++macroStage;
    if (macroStage == 3) { macroRelease = true; macroDue = millis() + 35; return; }
    if (macroDelays[macroStage]) { macroDue = millis() + macroDelays[macroStage]; return; }
  }
}

bool commitProfile(int index) {
  if (index < 0 || index >= profileCount) return false;
  // Parse separately: invalid input must not overwrite the running profile.
  std::unique_ptr<Profile> selected(new (std::nothrow) Profile);
  if (!selected || !readProfile(index, *selected)) return false;
  configAcknowledged.store(false); configBusy.store(true);
  uint32_t start = millis();
  while (!configAcknowledged.load() && millis() - start < 1000) delay(1);
  if (!configAcknowledged.load()) { configBusy.store(false); return false; }
  releaseKeys();
  profile = *selected;
  activeProfile = index;
  radioHUD.clear();
  chordRecognizer.cancel(buttonsMask); lastBindingMask = 0;
  setLEDProfile();
  FsFile last = SD.open("/last.txt", O_WRONLY | O_CREAT | O_TRUNC);
  if (last) { last.print(index); last.close(); }
  generation.fetch_add(1); wheelEvents.store(0);
  configBusy.store(false);
  return true;
}

void turnMenu(int steps) {
  if (!steps) return;
  if (menuKind==1) {
    menuSelection=constrain(menuSelection+steps,0,max(0,profileCount-1));
    settingsMenu.error=false;
  } else settingsMenu.turn(steps, profileCount);
}

void readButtons() {
  static bool raw[8] = {}, stable[8] = {}, longDone[2] = {};
  static uint32_t changed[8] = {}, pressed[2] = {};
  static bool releaseToArm = false;
  bool down[8] = {}, up[8] = {};
  uint32_t now = millis(); buttonsMask = 0;
  for (int i = 0; i < 8; ++i) {
    bool value = digitalRead(buttonPins[i]) == LOW;
    if (value != raw[i]) { raw[i] = value; changed[i] = now; }
    if (raw[i] != stable[i] && now - changed[i] >= 20) {
      stable[i] = raw[i]; down[i] = stable[i]; up[i] = !stable[i];
    }
    if (stable[i]) buttonsMask |= 1u << i;
  }
  bool asleep=idleTimer.update(now,rawTelemetry.load(),buttonsMask!=0);
  setDeviceStandby(asleep);
  if (asleep) return;
  for (int i = 0; i < 2; ++i) if (down[i+6]) { pressed[i]=now; longDone[i]=false; }
  if (menuOpen.load()) {
    chordRecognizer.cancel(buttonsMask);
    if (releaseToArm) {
      if (!stable[6] && !stable[7]) releaseToArm=false;
      return;
    }
    if (menuKind==1) {
      for (int i=0;i<6;++i) if (down[i]) menuSelection=constrain(menuSelection/6*6+i,0,max(0,profileCount-1));
      if ((up[6] && !longDone[0]) || (stable[6] && now-pressed[0]>=650)) {
        longDone[0]=longDone[1]=true;menuOpen.store(false);wheelEvents.store(0);generation.fetch_add(1);
      } else if (up[7] && !longDone[1]) {
        if (commitProfile(menuSelection)) {
          menuOpen.store(false); wheelEvents.store(0); generation.fetch_add(1);
          settingsMenu.error=false;
        } else settingsMenu.error=true;
        longDone[0]=longDone[1]=true;
      }
      return;
    }
    for (int i=0;i<2;++i) if (stable[i+6] && !longDone[i] && now-pressed[i]>=650) {
      longDone[0]=longDone[1]=true;
      if (i==0 || saveMenuSettings()) {
        menuOpen.store(false); wheelEvents.store(0); generation.fetch_add(1);
        chordRecognizer.cancel(buttonsMask);
      } else settingsMenu.error=true;
      return;
    }
    for (int i=0;i<6;++i) if (down[i]) settingsMenu.select(i);
    if (up[6] && !longDone[0]) settingsMenu.back();
    if (up[7] && !longDone[1]) {
      if (!settingsMenu.editing) settingsMenu.select(settingsMenu.item);
      else confirmMenuField();
    }
    return;
  }
  if (consumeFeiyuGesture((uint8_t)buttonsMask,now)) {
    chordRecognizer.cancel(buttonsMask);longDone[0]=longDone[1]=true;
    return;
  }
  if (profile.auxiliaryMode) {
    uint8_t gesture = chordRecognizer.update((uint8_t)buttonsMask, now, profile.chordWindow, profile.auxiliaryMode == 1);
    if (profile.auxiliaryMode == 1 && chordRecognizer.menuPair(buttonsMask, now)) {
      enterSettings(); releaseToArm = true;
      chordRecognizer.cancel(buttonsMask); wheelEvents.store(0);
      return;
    }
    if (gesture && sdDetected) startChord(gesture);
    return;
  }
  // Legacy small keys retain their exact original navigation behaviour.
  if (stable[6] || stable[7]) chordRecognizer.cancel(buttonsMask & 63);
  else if (profile.chordCount) {
    uint8_t gesture = chordRecognizer.update(buttonsMask & 63, now, profile.chordWindow);
    if (gesture && sdDetected) startChord(gesture);
  } else for (int i = 0; i < 6; ++i) if (down[i] && sdDetected) startMacro(i);
  if ((stable[6] && !longDone[0] && now - pressed[0] >= 650) ||
      (stable[7] && !longDone[1] && now - pressed[1] >= 650)) {
    if (stable[6] && !longDone[0] && now-pressed[0]>=650) enterProfileSelector();
    else enterSettings();
    releaseToArm = true;
    longDone[0] = longDone[1] = true; wheelEvents.store(0);
    return;
  }
  for (int i = 0; i < 2; ++i) if (up[i + 6] && !longDone[i]) {
    commitProfile((activeProfile + (i == 0 ? 1 : -1) + profileCount) % max(1,profileCount)); longDone[i] = true;
  }
}

void printJSONString(const char *s) {
  Serial.print('"');
  for (; *s; ++s) { if (*s == '"' || *s == '\\') Serial.print('\\'); if ((uint8_t)*s >= 32) Serial.print(*s); }
  Serial.print('"');
}
void serialCommand() {
  static char buffer[384]; static unsigned used = 0; static bool overflow = false;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c != '\n') { if (used < sizeof(buffer) - 1 && !overflow) buffer[used++] = c; else overflow = true; continue; }
    if (overflow) { used=0; overflow=false; Serial.println("HP ERROR LENGTH"); continue; }
    buffer[used] = 0; used = 0;
    if (handleFeiyuCommand(buffer)) {
      // File operations run only on this core, beside existing SD users.
    } else if (!strncmp(buffer, "HP HUD ", 7)) {
      Serial.println(radioHUD.update(buffer+7, profile.name, millis()) ? "HP OK HUD" : "HP ERROR HUD");
    } else if (!strcmp(buffer, "HP HUDOFF")) {
      radioHUD.clear(); Serial.println("HP OK HUDOFF");
    } else if (!strncmp(buffer,"HP PROFILE ",11)) {
      const char *number=buffer+11;char *end=nullptr;long index=strtol(number,&end,10);
      if(!*number || *number<'0' || *number>'9' || *end || index<0 || index>=profileCount || diskMode.load() || menuOpen.load())Serial.println("HP ERROR PROFILE");
      else Serial.println(commitProfile(index)?"HP OK PROFILE":"HP ERROR PROFILE");
    } else if (!strcmp(buffer, "HP INFO")) {
      Serial.print("HP INFO {\"version\":\""); Serial.print(FW_VERSION);
      Serial.print("\",\"profile\":"); printJSONString(profile.name);
      Serial.print(",\"config_storage\":\"sd-indexed\",\"profile_count\":");Serial.print(profileCount);
      Serial.print(",\"config_bytes\":");Serial.print(configIndex.bytes);
      Serial.print(",\"active_profile\":");Serial.print(activeProfile);
      Serial.print(",\"active_profile_bytes\":");Serial.print(activeProfile>=0 && unsigned(activeProfile)<configIndex.count?configIndex.profiles[activeProfile].size():0);
      Serial.print(",\"free_heap\":");Serial.print(rp2040.getFreeHeap());
      Serial.print(",\"storage_stack_min_free\":");Serial.print(minimumStorageStackFree);
      Serial.print(",\"sd\":"); Serial.print(sdDetected ? "true" : "false");
      Serial.print(",\"disk_mode\":"); Serial.print(diskMode.load() ? "true" : "false");
      Serial.print(",\"usb_mounted\":"); Serial.print(TinyUSBDevice.mounted() ? "true" : "false");
      Serial.print(",\"keyboard_ready\":"); Serial.print(usb_keyboard.ready() ? "true" : "false");
      Serial.print(",\"mouse_ready\":"); Serial.print(usb_mouse.ready() ? "true" : "false");
      Serial.print(",\"motor_requested\":"); Serial.print(wantMotor.load() ? "true" : "false");
      Serial.print(",\"motor_ready\":"); Serial.print(focReady.load() ? "true" : "false");
      Serial.print(",\"motor_paused\":"); Serial.print(motorPaused.load() ? "true" : "false");
      Serial.print(",\"sensor_direction\":"); Serial.print(sensorSign.load());
      Serial.print(",\"raw_angle\":"); Serial.print(rawTelemetry.load(), 5);
      Serial.print(",\"shaft_angle\":"); Serial.print(shaftTelemetry.load(), 5);
      Serial.print(",\"target\":"); Serial.print(targetTelemetry.load(), 5);
      Serial.print(",\"wheel_mode\":"); Serial.print((int)profile.mode);
      Serial.print(",\"feedback_mode\":"); Serial.print((int)profile.feedback.mode);
      Serial.print(",\"feedback_strength\":"); Serial.print(profile.feedback.strength);
      Serial.print(",\"feedback_voltage_cap\":"); Serial.print(profile.feedback.voltage, 2);
      Serial.print(",\"output_voltage\":"); Serial.print(outputTelemetry.load(), 3);
      Serial.print(",\"led_driver\":\"NeoPixel-PIO\",\"led_driver_ready\":"); Serial.print(ledTransport.ready() ? "true" : "false");
      Serial.print(",\"led_mode\":"); Serial.print(ledMode);
      Serial.print(",\"led_pixel\":"); Serial.print(ledPixel);
      Serial.print(",\"led_frames\":"); Serial.print(ledFrames);
      Serial.print(",\"flywheel_measured_rpm\":"); Serial.print(flywheelMeasuredRPM.load(), 2);
      Serial.print(",\"flywheel_peak_rpm\":"); Serial.print(flywheelPeakRPM.load(), 2);
      Serial.print(",\"flywheel_armed\":"); Serial.print(flywheelArmed.load()?"true":"false");
      Serial.print(",\"flywheel_boost_percent\":"); Serial.print(profile.flywheel.boostPercent);
      Serial.print(",\"flywheel_capture_rule\":\"drop_first_mean_2_3_else_2\",\"flywheel_sample_count\":"); Serial.print(flywheelSamples.load());
      Serial.print(",\"flywheel_state\":"); Serial.print(flywheelState.load());
      Serial.print(",\"flywheel_remaining_ms\":"); Serial.print(flywheelRemaining.load());
      Serial.print(",\"flywheel_load_stop\":"); Serial.print(flywheelLoadStop.load()?"true":"false");
      Serial.print(",\"pet_quote_id\":"); Serial.print(petQuoteId);
      Serial.print(",\"pet_quote_count\":"); Serial.print(petQuoteCount);
      Serial.print(",\"standby\":"); Serial.print(deviceStandby.load()?"true":"false");
      Serial.print(",\"idle_ms\":"); Serial.print(idleTimer.elapsed(millis()));
      Serial.print(",\"standby_after_ms\":180000");
      Serial.print(",\"uptime_ms\":"); Serial.print(millis());
      Serial.print(",\"hud_active\":"); Serial.print(radioHUD.fresh(millis()) ? "true" : "false");
      Serial.print(",\"reset_watchdog\":"); Serial.print(watchdog_caused_reboot() ? "true" : "false");
      Serial.print(",\"settings_open\":"); Serial.print(menuOpen.load() ? "true" : "false");
      Serial.print(",\"menu_kind\":"); Serial.print(menuKind);
      Serial.print(",\"settings_item\":"); Serial.print(settingsMenu.item);
      Serial.print(",\"menu_selection\":"); Serial.print(menuSelection);
      Serial.print(",\"menu_error\":"); Serial.print(settingsMenu.error ? "true" : "false");
      Serial.print(",\"chord_capacity\":255,\"chord_count\":"); Serial.print(profile.chordCount);
      Serial.print(",\"auxiliary_mode\":"); Serial.print(profile.auxiliaryMode);
      Serial.print(",\"last_binding_mask\":"); Serial.print(lastBindingMask);
      Serial.print(",\"last_binding_label\":"); printJSONString(lastBindingLabel);
      Serial.print(",\"buttons\":"); Serial.print(buttonsMask); Serial.println('}');
    } else if (!strcmp(buffer, "HP SLEEP")) {
      if (!diskMode.load()) { idleTimer.sleep(); setDeviceStandby(true); }
      Serial.println("HP OK SLEEP");
    } else if (!strcmp(buffer, "HP WAKE")) {
      if (!diskMode.load()) { idleTimer.wake(millis(),rawTelemetry.load()); setDeviceStandby(false); }
      Serial.println("HP OK WAKE");
    } else if (!strcmp(buffer, "HP MOTOR 0")) {
      wantMotor.store(false); Serial.println("HP OK MOTOR 0");
    } else if (!strcmp(buffer, "HP MOTOR 1")) {
      wantMotor.store(true); generation.fetch_add(1); Serial.println("HP OK MOTOR 1");
    } else if (!strcmp(buffer, "HP SDMODE")) {
      releaseKeys(); wantMotor.store(false);
      Serial.println("HP OK SDMODE"); Serial.flush(); delay(50);
      watchdog_hw->scratch[0] = SD_BOOT_MAGIC; rp2040.reboot();
    } else if (!strcmp(buffer, "HP BOOT")) {
      releaseKeys(); wantMotor.store(false);
      Serial.println("HP OK BOOT"); Serial.flush(); delay(50); reset_usb_boot(0, 0);
    } else if (!strcmp(buffer, "HP REBOOT")) {
      Serial.println("HP OK REBOOT"); Serial.flush(); delay(50); rp2040.reboot();
    }
    break;
  }
}

void loop() {
  if (diskMode.load()) {
    serialCommand();
    if (digitalRead(9) == LOW) rp2040.reboot();
    return;
  }
  motorTick();
}
void loop1() {
  if (diskMode.load()) return;
  serialCommand(); readButtons();
  tickFeiyu();
  if (deviceStandby.load()) return;
  tickMacro();
  static uint32_t wheelKeyUntil = 0;
  int steps = wheelEvents.exchange(0);
  if (menuOpen.load()) {
    turnMenu(steps);
  } else if (steps && sdDetected && usb_mouse.ready()) {
    int scroll = constrain(-steps * profile.direction, -127, 127);
    if (!macroActive && profile.wheelKey && usb_keyboard.ready()) {
      uint8_t keys[6] = {};
      int modifier = checkModifiers(profile.wheelKey);
      if (!modifier) keys[0] = convertKeycode(profile.wheelKey);
      usb_keyboard.keyboardReport(0, modifier, keys); wheelKeyUntil = millis() + 120;
    }
    usb_mouse.mouseScroll(0, scroll, 0);
  }
  if (wheelKeyUntil && (int32_t)(millis() - wheelKeyUntil) >= 0 && !macroActive) {
    releaseKeys(); wheelKeyUntil = 0;
  }
  static uint32_t ledAt = 0, frameAt = 0;
  if (millis() - ledAt >= ((ledMode == 6 || ledMode == 7) ? 20u : 50u)) {
    ledAt = millis();
    if (ledMode != 6 && ledMode != 7) ledPixel = -1;
    switch (ledMode) {
      case 0: haloLED(); break; case 1: breathLED(); break;
      case 2: ledBand(); break; case 3: rainbowLED(); break;
      case 4: solidLED(); break; case 6: knobLED(); break; case 7: knobTrailLED(); break; default: offLED();
    }
  }
  if (millis() - frameAt >= 100) {
    frameAt = millis(); screenFrameMs = frameAt; updatePetSpeech(frameAt); u8g2.clearBuffer();
    drawScreen();
    // Send one 8-pixel strip at a time, sampling buttons between strips.
    // A short tap must not wait for a complete OLED transfer.
    for (uint8_t row=0;row<8;++row) {
      u8g2.updateDisplayArea(0,row,16,1);
      readButtons(); tickMacro();
    }
  }
}
