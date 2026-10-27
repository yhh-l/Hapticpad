#include <cassert>
#include <iostream>
#include <limits>
#include "../firmware/MacroPad/led_trail.h"
bool same(TrailRGB a,TrailRGB b) {return a.r==b.r && a.g==b.g && a.b==b.b;}
int main() {
  KnobColourTrail<20> trail; trail.reset(123);
  TrailRGB bg{0,4,12};
  for(int i=0;i<20;++i) assert(same(trail.pixel(i,0,bg),bg));
  assert(!trail.moved(0));
  for(int i=0;i<100;++i) assert(!trail.moved(i%2?.0015f:-.0015f));
  assert(trail.moved(.01f));
  trail.update(0,true,10); auto first=trail.pixel(0,10,bg);
  assert(!same(first,bg));
  trail.update(0,true,100); assert(same(first,trail.pixel(0,100,bg)));
  trail.update(1,true,150); auto second=trail.pixel(1,150,bg);
  assert(!same(first,second)); assert(same(first,trail.pixel(0,150,bg)));
  auto half=trail.pixel(0,1150,bg);
  assert(half.r==(first.r+bg.r)/2 && half.g==(first.g+bg.g)/2 && half.b==(first.b+bg.b)/2);
  assert(same(trail.pixel(0,2150,bg),bg));
  assert(same(trail.pixel(1,2150,bg),bg)); // Stop leaves a uniform background.
  trail.update(2,true,2200); assert(same(trail.pixel(1,2200,bg),bg));
  auto third=trail.pixel(2,2200,bg); trail.update(1,true,2220);
  assert(!same(third,trail.pixel(1,2220,bg)));
  for(int i=0;i<20;++i) assert(same(trail.pixel(i,5000,{}),{}));
  trail.reset(); for(int i=0;i<20;++i) assert(same(trail.pixel(i,5001,bg),bg));
  assert(!trail.moved(std::numeric_limits<float>::quiet_NaN()));
  trail.update(19,true,0xfffffff0u);auto wrap=trail.pixel(19,0xfffffff0u,bg);
  assert(!same(wrap,bg));assert(same(trail.pixel(19,uint32_t(0xfffffff0u+2000u),bg),bg));
  std::cout<<"PASS: startup/idle, random colours per position, no recolour in sector, independent 2s fade, resume/reverse, black background, reset and timer wrap\n";
}
