#include <cassert>
#include <cstdint>
#include <iostream>
#include "../firmware/MacroPad/chords.h"
int main() {
  for (int mask=1;mask<=255;++mask) {
    ChordRecognizer keys;
    assert(!keys.update(mask,10,100));
    assert(keys.update(mask,110,100)==mask);
    assert(!keys.update(mask,400,100));
    assert(!keys.update(0,410,100));
    assert(!keys.update(mask,500,100));
    assert(keys.update(0,520,100)==mask);
  }
  ChordRecognizer keys;
  assert(!keys.update(1,0,100));
  assert(!keys.update(3,30,100));
  assert(!keys.update(7,70,100));
  assert(keys.update(7,100,100)==7);
  assert(!keys.update(3,110,100));assert(!keys.update(1,120,100));assert(!keys.update(0,130,100));
  assert(!keys.update(3,200,100));assert(!keys.update(1,230,100));assert(keys.update(0,250,100)==3);
  assert(!keys.update(1,300,100));assert(keys.update(3,401,100)==1); // Late join cannot change a resolved gesture.
  assert(!keys.update(0,450,100));
  keys.cancel(7);assert(!keys.update(7,800,100));assert(!keys.update(0,810,100));
  assert(!keys.update(128,900,100));assert(keys.update(0,920,100)==128);
  keys.cancel();assert(!keys.update(64,1000,100,true));assert(!keys.update(192,1030,100,true));
  assert(!keys.update(192,1200,100,true));assert(!keys.menuPair(192,1799));assert(keys.menuPair(192,1800));
  keys.cancel(192);assert(!keys.update(0,1810,100,true));
  assert(!keys.update(192,2000,100,true));assert(keys.update(0,2300,100,true)==192);
  keys.cancel();assert(!keys.update(192,3000,100,false));assert(keys.update(192,3100,100,false)==192);assert(!keys.menuPair(192,4000));
  keys.cancel();assert(!keys.update(255,UINT32_MAX-49,100));assert(keys.update(255,50,100)==255);
  std::cout<<"PASS: all 255 held/tapped masks, exact chords, partial release, single suppression, late joins, held cancellation, custom menu pair and timer wrap\n";
}
