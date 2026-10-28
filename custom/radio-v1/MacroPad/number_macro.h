#pragma once
#include <cstdint>
#include <cstring>
// Each digit has its own press and release, with no OS modifier or clipboard.
class NumberMacro {
  char digits_[16]={};
  uint8_t index_=0,length_=0;
  bool running_=false,release_=false,enter_=false;
  uint32_t due_=0;
 public:
  bool active() const {return running_;}
  void cancel(){running_=false;release_=false;}
  bool begin(const char *text,bool enter,uint32_t now){
    if(running_ || !text)return false;
    unsigned n=std::strlen(text);
    if(!n || n>=sizeof(digits_))return false;
    for(unsigned i=0;i<n;++i)if(text[i]<'0'||text[i]>'9')return false;
    std::memcpy(digits_,text,n+1);index_=0;length_=n;enter_=enter;
    running_=true;release_=false;due_=now;return true;
  }
  // 0 no event, 1 key down (Windows VK), 2 all keys up.
  int tick(uint32_t now,uint8_t &vk){
    if(!running_ || int32_t(now-due_)<0)return 0;
    if(release_){
      release_=false;
      if(index_>=length_+unsigned(enter_))running_=false;
      due_=now+20;return 2;
    }
    vk=index_<length_?uint8_t(digits_[index_]):13;
    ++index_;release_=true;due_=now+30;return 1;
  }
};
