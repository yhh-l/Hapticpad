#include <cassert>
#include <iostream>
#include "feiyu_assistant.h"
int main(){
  FeiyuAssistant ai;
  assert(!ai.buttons(1,10));assert(!ai.mode);
  assert(ai.buttons(41,100));assert(!ai.mode);
  assert(ai.buttons(41,1100));assert(ai.mode&&ai.count==1);
  assert(ai.buttons(41,4100));assert(ai.count==1);
  assert(ai.buttons(9,4200));assert(ai.mode);
  ai.buttons(0,4300);assert(ai.buttons(2,4400));assert(ai.count==2);
  ai.ack(999);assert(ai.count==2);ai.ack(ai.queue[ai.head].id);assert(ai.count==1);
  ai.turn(1);assert(ai.count==2);
  ai.buttons(0,4500);ai.buttons(32,4600);assert(!ai.mode);
  assert(ai.status("thinking",0xfffffff0));assert(ai.fresh(32));assert(!ai.fresh(7000));
  assert(!ai.status("pretend-success",7001));
  for(int i=0;i<30;++i){ai.emit("open");}
  assert(ai.count==8);
  FeiyuAssistant wrap;wrap.buttons(41,0xfffffff0);wrap.buttons(41,1010);assert(wrap.mode);
  std::cout<<"PASS assistant mode, release gating, bounded events, expiry and clock wrap\n";
}
