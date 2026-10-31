#pragma once
#include <atomic>
#include <memory>
#include "sd_config_index.h"
#include "chords.h"
#include "settings_menu.h"
#include "haptics.h"
#include "led_follow.h"
#include "led_trail.h"
#include "flywheel.h"
#include "pixel_pet.h"
#include "idle_timer.h"

constexpr unsigned MAX_PROFILES = SD_MAX_PROFILES;
constexpr uint32_t SD_BOOT_MAGIC = 0x4850434E;
constexpr const char *FW_VERSION = "V1-CN-1.12.0-FEIYU";
struct ChordBinding {
  uint8_t mask = 0;
  char numberText[16] = {};
  bool enterAfterText = false;
  char label[32] = {};
  uint8_t actions[3] = {};
  uint16_t delays[3] = {};
};
struct Profile {
  char name[32] = {};
  char displayText[32] = {};
  uint8_t displayScene = 1; // 0=text, 1=dog, 2=person, 3=blank
  char labels[8][32] = {};
  uint8_t actions[8][3] = {};
  uint16_t delays[8][3] = {};
  ChordBinding chords[247];
  unsigned chordCount = 0;
  uint16_t chordWindow = 100;
  uint8_t auxiliaryMode = 0; // 0 legacy; 1 custom + long 7+8 menu; 2 all custom.
  FeedbackSettings feedback;
  FlywheelSettings flywheel;
  uint8_t mode = 0;
  uint8_t wheelKey = 0;
  int direction = 1;
  int ledMode = 4;
  int brightness = 35;
  int ledDirection = 1, ledOffset = 0;
  uint8_t primary[3] = {0, 48, 120};
  uint8_t secondary[3] = {};
};
// Motor targets must use the same signed shaft coordinate as SimpleFOC.
inline float shaftCoordinate(float raw, int direction, float offset) {
  return raw * direction - offset;
}
