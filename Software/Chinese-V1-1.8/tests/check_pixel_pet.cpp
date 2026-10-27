#include <cassert>
#include <cstdint>
#include <set>
#include <utility>
#include "pixel_pet.h"
int main() {
  for (bool person : {false,true}) {
    std::set<int> positions;
    std::set<int> heights;
    for (uint32_t now=0; now<120000; now+=60) {
      std::set<std::pair<int,int>> first,second;
      auto plot=[&](int x,int y){assert(x>=3 && x<=124);assert(y>=17 && y<=44);assert(y>=31 || x<118);first.emplace(x,y);positions.insert(x);heights.insert(y);};
      drawPixelPet(plot,now,person);
      drawPixelPet([&](int x,int y){second.emplace(x,y);},now,person);
      assert(!first.empty() && first==second); // All paged OLED passes must agree.
    }
    assert(positions.size()>90 && heights.size()>20);
    for(uint32_t now=0;now<5000;now+=60)
      drawPixelPet([](int x,int y){assert(x>=103&&x<=122&&y>=33&&y<=44);},now,person,true);
    for(uint32_t now : {UINT32_MAX-59,UINT32_MAX,0u})
      drawPixelPet([](int x,int y){assert(x>=3&&x<=124&&y>=17&&y<=44);},now,person);
  }
}
