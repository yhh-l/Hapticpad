#pragma once
#include <cmath>

// Map a relative quadrature angle to one LED per sector. A small hysteresis
// prevents a stationary knob at a sector boundary from toggling adjacent LEDs.
class KnobPixelTracker {
 public:
  int update(float radians, int count, int direction, int offset) {
    if (count <= 0) return 0;
    if (!std::isfinite(radians)) return index_ < 0 ? offset : mapped(count,direction,offset);
    constexpr float turn = 6.283185307179586f;
    float position = std::fmod(radians,turn) * count / turn;
    if (position < 0) position += count;
    if (index_ < 0) index_ = int(std::lround(position)) % count;
    float delta = position - index_;
    if (delta > count / 2.f) delta -= count;
    if (delta < -count / 2.f) delta += count;
    if (std::fabs(delta) > .65f) index_ = int(std::lround(position)) % count;
    return mapped(count,direction,offset);
  }
 private:
  int index_ = -1;
  int mapped(int count, int direction, int offset) const {
    int value = (index_ * direction + offset) % count;
    return value < 0 ? value + count : value;
  }
};
