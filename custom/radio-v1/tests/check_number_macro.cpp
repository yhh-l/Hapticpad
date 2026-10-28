#include <cassert>
#include <vector>
#include "number_macro.h"
int main(){
 NumberMacro m;uint8_t vk=0;
 assert(!m.begin("",true,0));assert(!m.begin("1234567890123456",true,0));assert(!m.begin("12x",true,0));
 assert(m.begin("8811",true,0));assert(!m.begin("123",true,0));
 std::vector<int> keys;int releases=0;bool down=false;
 for(uint32_t t=0;t<400;++t){int event=m.tick(t,vk);if(event==1){assert(!down);down=true;keys.push_back(vk);}if(event==2){assert(down);down=false;++releases;}}
 assert((keys==std::vector<int>{56,56,49,49,13}));assert(releases==5&&!m.active()&&!down);
 assert(m.begin("1",false,0xfffffff0));assert(m.tick(0xfffffff0,vk)==1&&vk==49);assert(m.tick(20,vk)==2&&!m.active());
 assert(m.begin("123",true,30));m.cancel();assert(m.tick(100,vk)==0);
}
