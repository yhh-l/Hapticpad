#include <cassert>
#include "radio_hud.h"
int main(){
 RadioHUD h;
 assert(!h.valid);
 assert(h.update("onair|14200000|USB|100|42|0","OnAir收听",100));
 assert(h.frequency==14200000 && h.volume==42 && !h.muted && h.step==100);
 assert(h.fresh(5099) && !h.fresh(5100));
 assert(!h.update("nexus|14200000|USB|100|42|0","OnAir收听",200));
 assert(!h.update("onair|0|USB|100|42|0","OnAir收听",200));
 assert(!h.update("onair|14200000|USB|100|101|0","OnAir收听",200));
 assert(!h.update("onair|14200000|USB|100|-2|0","OnAir收听",200));
 assert(!h.update("onair|14200000|USB|100|42|0|extra","OnAir收听",200));
 assert(!h.update("onair|14200000|USB|100|42","OnAir收听",200));
 assert(!h.update("onair|14200000|US B|100|42|0","OnAir收听",200));
 assert(!h.update("onair|999999999999999999999|USB|100|42|0","OnAir收听",200));
 assert(h.update("neuro|91000|LSB|100000|-1|0","NeuroSDR收听",0xfffffff0));
 assert(h.volume==-1 && h.fresh(20));
 h.clear();assert(!h.fresh(20));
}
