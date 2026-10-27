#pragma once
#include <cstdint>
#include <cmath>
class IdleTimer {
  uint32_t last=0;
  float anchor=0;
  bool initialized=false, asleep=false;
public:
  static constexpr uint32_t TIMEOUT_MS=180000;
  void wake(uint32_t now,float angle){last=now;if(std::isfinite(angle))anchor=angle;initialized=true;asleep=false;}
  void sleep(){asleep=true;}
  bool update(uint32_t now,float angle,bool buttons){
    if(!initialized)wake(now,angle);
    bool moved=std::isfinite(angle)&&std::fabs(angle-anchor)>=.02f;
    if(buttons||moved)wake(now,angle);
    if(uint32_t(now-last)>=TIMEOUT_MS)asleep=true;
    return asleep;
  }
  uint32_t elapsed(uint32_t now)const{return now-last;}
};
