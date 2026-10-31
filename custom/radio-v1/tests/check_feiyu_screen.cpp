#include <cassert>
#include <cstdint>
#include <set>
#include <utility>
#include "feiyu_screen.h"

int main() {
  std::set<unsigned> poses;
  for(uint32_t origin : {0u,UINT32_MAX-4400}) {
    for(uint32_t elapsed=0;elapsed<9000;elapsed+=50) {
      for(uint32_t idle : {0u,59999u,60000u,179999u}) {
        for(bool active : {false,true}) {
          const auto frame=feiyuScreenFrame(origin+elapsed,idle,active);
          poses.insert(frame.pose);
          std::set<std::pair<int,int>> pixels;
          drawFeiyuPixels([&](int x,int y){
            assert(x>=0 && x<128 && y>=0 && y<64);
            assert(pixels.insert({x,y}).second);
          },frame);
          assert(pixels.size()>200 && pixels.size()<2000);
          if(!active && idle>=60000)assert(frame.pose==FEIYU_SLEEPY);
        }
      }
    }
  }
  assert(poses.count(FEIYU_IDLE) && poses.count(FEIYU_BLINK) && poses.count(FEIYU_CLOSED));
  assert(poses.count(FEIYU_HAPPY) && poses.count(FEIYU_WAVE) && poses.count(FEIYU_SLEEPY));
  for(unsigned pose=0;pose<FEIYU_SPRITE_COUNT;++pose) {
    FeiyuScreenFrame frame;frame.pose=pose;frame.x=-20;frame.y=-10;
    drawFeiyuPixels([](int x,int y){assert(x>=0 && x<128 && y>=0 && y<64);},frame);
  }
}
