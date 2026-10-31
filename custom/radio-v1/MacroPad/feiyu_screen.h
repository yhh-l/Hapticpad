#pragma once
#include <cstdint>
#include "feiyu_sprites.h"

struct FeiyuScreenFrame {
  uint8_t pose=FEIYU_IDLE;
  int x=50, y=17;
};

inline FeiyuScreenFrame feiyuScreenFrame(uint32_t now, uint32_t idleMs, bool reacting) {
  FeiyuScreenFrame frame;
  frame.y += (now / 700) % 2;
  if (reacting) {
    frame.pose = (now / 350) % 2 ? FEIYU_HAPPY : FEIYU_WAVE;
    frame.x += (now / 250) % 2 ? 1 : -1;
  } else if (idleMs >= 60000) {
    frame.pose = FEIYU_SLEEPY;
  } else {
    const uint32_t phase = now % 4300;
    if (phase >= 3800 && phase < 3900) frame.pose=FEIYU_BLINK;
    else if (phase >= 3900 && phase < 4100) frame.pose=FEIYU_CLOSED;
    else if (phase >= 4100 && phase < 4200) frame.pose=FEIYU_BLINK;
  }
  return frame;
}

template<class Pixel>
void drawFeiyuPixels(Pixel pixel, const FeiyuScreenFrame &frame) {
  if (frame.pose >= FEIYU_SPRITE_COUNT) return;
  for (unsigned y=0; y<FEIYU_SPRITE_HEIGHT/2; ++y) {
    for (unsigned x=0; x<FEIYU_SPRITE_WIDTH/2; ++x) {
      const int dx=frame.x+int(x), dy=frame.y+int(y);
      if (dx<0 || dx>=128 || dy<0 || dy>=64) continue;
      bool lit=false;
      for(unsigned oy=0;oy<2;++oy)for(unsigned ox=0;ox<2;++ox){
        unsigned sx=x*2+ox,sy=y*2+oy;
        lit=lit || (feiyuSpriteBits[frame.pose][sy*FEIYU_SPRITE_STRIDE+sx/8] & (1u<<(sx%8)));
      }
      if(lit)pixel(dx,dy);
    }
  }
}
