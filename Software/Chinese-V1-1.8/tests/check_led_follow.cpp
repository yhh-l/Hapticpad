#include <cassert>
#include <limits>
#include <iostream>
#include "../firmware/MacroPad/led_follow.h"
int main() {
  constexpr float turn=6.283185307179586f, sector=turn/20;
  KnobPixelTracker tracker;
  for(int i=0;i<=100;++i) assert(tracker.update(i*sector,20,1,0)==i%20);
  for(int i=100;i>=-100;--i) assert(tracker.update(i*sector,20,1,0)==(i%20+20)%20);
  assert(tracker.update(0,20,1,0)==0);
  for(int i=0;i<1000;++i) assert(tracker.update((i%2?.51f:.49f)*sector,20,1,0)==0);
  assert(tracker.update(.7f*sector,20,1,0)==1);
  for(int i=0;i<1000;++i) assert(tracker.update((i%2?.49f:.51f)*sector,20,1,0)==1);
  assert(tracker.update(.3f*sector,20,1,0)==0);
  assert(tracker.update(-.7f*sector,20,1,0)==19);
  assert(tracker.update(.7f*sector,20,-1,3)==2);
  assert(tracker.update(7*sector,20,-1,3)==16);
  assert(tracker.update(std::numeric_limits<float>::quiet_NaN(),20,-1,3)==16);
  KnobPixelTracker startup;
  assert(startup.update(std::numeric_limits<float>::infinity(),20,1,5)==5);
  assert(startup.update(10.25f*turn,20,1,0)==5);
  std::cout<<"PASS: angle tracking, reverse, wrap, multiple turns, jitter hysteresis, offset and invalid angles\n";
}
