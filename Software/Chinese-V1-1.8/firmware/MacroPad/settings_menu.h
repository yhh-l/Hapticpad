#pragma once
#include <cstdint>
struct MenuValues {
  int profile=0, strength=25, damping=6, led=7, brightness=35, wheel=0;
  bool follow=true;
};
class SettingsMenu {
 public:
  MenuValues values;
  int item=0;
  bool editing=false, error=false;
  void begin(const MenuValues &initial){values=initial;item=0;editing=false;error=false;}
  void select(int index){item=index;editing=true;original_=field();error=false;}
  void confirm(){editing=false;error=false;}
  void back(){if(editing)field()=original_;editing=false;error=false;}
  void turn(int delta,int count){
    error=false;
    if(!editing){item=bound(item+delta,0,5);return;}
    int &value=field();
    switch(item){
      case 0:value=bound(value+delta,0,count-1);break;
      case 1:case 2:value=bound(value+delta*5,0,100);break;
      case 3:{const int modes[]={5,4,7,6,0,1,2,3};int position=0;for(int i=0;i<8;i++)if(modes[i]==value)position=i;value=modes[bound(position+delta,0,7)];break;}
      case 4:value=bound(value+delta*5,0,255);break;
      case 5:value=values.follow?bound(value+delta,0,3):0;break;
    }
  }
 private:
  int original_=0;
  static int bound(int value,int lo,int hi){return value<lo?lo:value>hi?hi:value;}
  int &field(){switch(item){case 0:return values.profile;case 1:return values.strength;case 2:return values.damping;case 3:return values.led;case 4:return values.brightness;default:return values.wheel;}}
};
