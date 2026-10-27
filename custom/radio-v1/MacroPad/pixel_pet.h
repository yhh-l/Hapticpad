#pragma once
#include <cstdint>
// Stateless animation: all OLED pages use one captured frame timestamp.
inline uint32_t petHash(uint32_t n) {
  n ^= n >> 16; n *= 0x7feb352dU; n ^= n >> 15; return n;
}
struct PetPosition { int x, y; };
inline PetPosition petWaypoint(uint32_t n) {
  uint32_t h = petHash(n + 71);
  // The upper-left profile label and upper-right N/F badge are reserved.
  return n % 2 ? PetPosition{3 + int(h % 93), 18 + int((h >> 8) % 7)}
               : PetPosition{3 + int(h % 103), 33};
}
inline PetPosition petPosition(uint32_t now) {
  const uint32_t segment = now / 2400, phase = now % 2400;
  const PetPosition a = petWaypoint(segment), b = petWaypoint(segment + 1);
  const int t = phase < 2200 ? int(phase) : 2200;
  PetPosition position = {a.x + (b.x-a.x)*t/2200, a.y + (b.y-a.y)*t/2200};
  if (position.y < 31 && position.x > 98) position.x=98; // Keep clear of N/F.
  return position;
}
template<class Plot> void drawPixelPet(Plot plot, uint32_t now, bool person, bool talking = false) {
  const PetPosition position = talking ? PetPosition{103,33} : petPosition(now);
  const uint32_t segment = now / 2400, phase = now % 2400;
  const bool moving = !talking && phase < 2200;
  const bool flip = talking || petWaypoint(segment + 1).x < petWaypoint(segment).x;
  const int x = position.x;
  const unsigned gait = moving ? (now / 140) % 2 : 0;
  static const char *dog[] = {
    "             ##", "            ####", "            # ##",
    "    ########### ###", "   ###############",
    "  ##############", "  #############", "   ###########",
    "    ##     ##", "    ##     ##", "   ##     ##", ""
  };
  static const char *human[] = {
    "   ###", "  #####", "  ## ##", "   ###", "    #",
    "  #####", " #  #  #", "    #", "    #",
    "   # #", "  #   #", " ##   ##", ""
  };
  const int width = person ? 9 : 20;
  for (int row = 0; row < 12; ++row) {
    const char *line = person ? human[row] : dog[row];
    for (int col = 0; line[col]; ++col) {
      if (line[col] != '#') continue;
      int px = col;
      if (gait && row >= (person ? 9 : 8)) px += (col < width / 2 ? 1 : -1);
      plot(x + (flip ? width - 1 - px : px), position.y + row - (moving && gait ? 1 : 0));
    }
  }
  if (!person) { // Wagging tail, including while the dog is resting.
    int tailY = position.y + 3 + (now / 230) % 2;
    plot(x + (flip ? 19 : 0), tailY);
    plot(x + (flip ? 18 : 1), tailY + 1);
  }
}
