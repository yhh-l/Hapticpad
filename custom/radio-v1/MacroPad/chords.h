#pragma once
#include <cstdint>

// One output per press/release gesture; exact masks only, no subset fallback.
// A short tap resolves on release. Held gestures resolve after the join window.
class ChordRecognizer {
 public:
  void cancel(uint8_t held = 0) {
    active_ = held != 0; fired_ = active_; candidate_ = 0;
  }
  uint8_t update(uint8_t mask, uint32_t now, uint16_t window, bool deferPair = false) {
    if (!active_) {
      if (!mask) return 0;
      active_ = true; fired_ = false; since_ = now; candidate_ = mask;
    }
    if (fired_) { if (!mask) active_ = false; return 0; }
    if (!mask) {
      active_ = false; fired_ = true; return candidate_;
    }
    // Only enlarge a still-held chord. Releasing one constituent cannot turn
    // an unbound chord into an accidental single-key command.
    if (uint32_t(now - since_) < window && (mask & candidate_) == candidate_)
      candidate_ = mask;
    if (uint32_t(now - since_) < window || (deferPair && candidate_ == 192)) return 0;
    fired_ = true; return candidate_;
  }
  bool menuPair(uint8_t mask, uint32_t now) const {
    return active_ && !fired_ && candidate_ == 192 && mask == 192 && uint32_t(now - since_) >= 800;
  }
 private:
  uint32_t since_ = 0;
  uint8_t candidate_ = 0;
  bool active_ = false, fired_ = false;
};
