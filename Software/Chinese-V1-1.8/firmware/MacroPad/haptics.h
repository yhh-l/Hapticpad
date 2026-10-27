#pragma once
#include <cmath>
#include <cstdint>

enum FeedbackMode : uint8_t { FOLLOW = 0, DAMPING = 1, ASSIST = 2, FREE = 3 };
struct FeedbackSettings {
  uint8_t mode = FOLLOW;
  int strength = 25, damping = 6, assist = 20, detents = 40;
  float voltage = .8f;
  int speedRPM = 60, assistMs = 250, triggerRPM = 8;
};
inline float boundedVoltage(float value, float limit) {
  if (!std::isfinite(value) || !std::isfinite(limit) || limit <= 0) return 0;
  limit = limit > 3 ? 3 : limit;
  return value > limit ? limit : (value < -limit ? -limit : value);
}
inline float dampingVoltage(const FeedbackSettings &s, float rawVelocity) {
  return boundedVoltage(-rawVelocity * s.damping * .003f, s.voltage);
}
// Assistance is a finite pulse. It must see a fresh rest before another push.
// Encoder motion alone cannot tell a human push from a powered rotation.
class AssistPulse {
  bool armed = false, active = false, resting = false, moving = false;
  uint32_t restAt = 0, moveAt = 0, started = 0;
  int direction = 0;
public:
  void reset() { armed = active = resting = moving = false; direction = 0; }
  float update(const FeedbackSettings &s, float velocity, uint32_t now, bool allowed) {
    if (!allowed || !std::isfinite(velocity) || s.assist <= 0 || s.voltage <= 0) { reset(); return 0; }
    const float speed = fabsf(velocity), cutoff = s.speedRPM * .104719755f;
    if (speed < .2f) {
      active = false; moving = false;
      if (!resting) { resting = true; restAt = now; }
      if (uint32_t(now - restAt) >= 250) armed = true;
      return 0;
    }
    resting = false;
    if (active) {
      uint32_t elapsed = now - started;
      if (elapsed >= uint32_t(s.assistMs) || velocity * direction <= 0 || speed >= cutoff) {
        active = armed = moving = false; return 0;
      }
      float rise = elapsed < 35 ? elapsed / 35.f : 1.f;
      float remaining = s.assistMs - elapsed;
      float fade = remaining < 50 ? remaining / 50.f : 1.f;
      float cap = s.voltage < .9f ? s.voltage : .9f;
      return boundedVoltage(direction * cap * s.assist / 100.f * rise * fade * (1 - speed / cutoff), cap);
    }
    if (armed && speed >= s.triggerRPM * .104719755f && speed < cutoff) {
      const int currentDirection = velocity > 0 ? 1 : -1;
      if (!moving || direction != currentDirection) { moving = true; moveAt = now; direction = currentDirection; }
      if (uint32_t(now - moveAt) >= 40) { started = now; active = true; armed = moving = false; }
    } else moving = false;
    return 0;
  }
};
