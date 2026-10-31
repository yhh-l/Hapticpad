#pragma once
#include <cstdint>
#include <cstring>

struct FeiyuAssistant {
  bool mode=false,armed=true,gesture=false,fired=false,seen=false;
  uint32_t pressed=0,hostAt=0,nextId=0;
  char phase[16]="offline", page[128]={};
  unsigned pageIndex=0,pageCount=1;
  struct Event {uint32_t id;const char *action;};
  Event queue[8]={};unsigned head=0,count=0;
  bool fresh(uint32_t now)const{return seen && uint32_t(now-hostAt)<6000;}
  void emit(const char *action){if(count>=8)return;if(!++nextId)++nextId;queue[(head+count)%8]={nextId,action};++count;}
  void ack(uint32_t id){if(count && queue[head].id==id){head=(head+1)%8;--count;}}
  void turn(int steps){if(!mode || !steps)return;emit(steps>0?"next":"previous");}
  bool buttons(uint8_t mask,uint32_t now){
    if(!mask){armed=true;gesture=fired=false;return false;}
    if(mask==41 && !gesture && !fired){gesture=true;pressed=now;}
    if(gesture){
      if(mask==41 && !fired && uint32_t(now-pressed)>=1000){mode=!mode;fired=true;armed=false;emit(mode?"open":"exit");}
      // Partial release must not leak an original macro.
      return true;
    }
    if(!mode)return false;
    if(!armed)return true;
    armed=false;
    if(mask==1)emit("open");
    else if(mask==2)emit("resume");
    else if(mask==4)emit("save");
    else if(mask==8)emit("previous");
    else if(mask==16)emit("next");
    else if(mask==32){mode=false;emit("exit");}
    return true;
  }
  bool status(const char *value,uint32_t now){
    const char *valid[]={"idle","thinking","reply","tool","done","cancelled","error","saving","offline"};
    bool ok=false;for(auto p:valid)if(!std::strcmp(p,value))ok=true;
    if(!ok)return false;
    std::strcpy(phase,value);seen=true;hostAt=now;return true;
  }
};
