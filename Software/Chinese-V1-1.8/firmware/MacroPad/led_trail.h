#pragma once
#include <cstdint>
#include <cmath>

struct TrailRGB { uint8_t r=0, g=0, b=0; };

template<int Count> class KnobColourTrail {
 public:
  static constexpr uint32_t FadeMs = 2000;
  void reset(uint32_t seed=1) {
    for(int i=0;i<Count;++i) active_[i]=false;
    head_=-1; angleReady_=false; previousColour_=-1; random_=seed?seed:1;
  }
  // Accumulate small encoder increments; ignore stationary quantization noise.
  bool moved(float angle) {
    if(!std::isfinite(angle)) return false;
    if(!angleReady_) { angle_=angle; angleReady_=true; return false; }
    if(std::fabs(angle-angle_) < .0046f) return false;
    angle_=angle; return true;
  }
  void update(int pixel, bool moving, uint32_t now) {
    if(!moving || pixel<0 || pixel>=Count) return;
    if(pixel!=head_ || !active_[pixel] || uint32_t(now-stamp_[pixel])>=FadeMs) {
      // The pixel we just left starts its fade at full intensity if the knob
      // was moving; never resurrect an old tail when resuming after a pause.
      if(head_>=0 && active_[head_] && uint32_t(now-stamp_[head_])<100)
        stamp_[head_]=now;
      colour_[pixel]=nextColour(); active_[pixel]=true; head_=pixel;
    }
    stamp_[pixel]=now;
  }
  TrailRGB pixel(int index, uint32_t now, TrailRGB background) const {
    if(!active_[index]) return background;
    uint32_t age=now-stamp_[index];
    if(age>=FadeMs) return background;
    uint32_t weight=FadeMs-age;
    const auto &c=colour_[index];
    return {blend(c.r,background.r,weight),blend(c.g,background.g,weight),blend(c.b,background.b,weight)};
  }
 private:
  bool active_[Count]={}, angleReady_=false;
  uint32_t stamp_[Count]={}, random_=1;
  TrailRGB colour_[Count];
  float angle_=0;
  int head_=-1, previousColour_=-1;
  static uint8_t blend(uint8_t a,uint8_t b,uint32_t weight) {
    return (uint32_t(a)*weight+uint32_t(b)*(FadeMs-weight))/FadeMs;
  }
  TrailRGB nextColour() {
    static constexpr TrailRGB palette[]={
      {255,35,20},{255,140,0},{255,225,20},{40,255,60},{0,255,180},
      {0,180,255},{30,70,255},{130,35,255},{230,40,255},{255,35,120}
    };
    random_^=random_<<13; random_^=random_>>17; random_^=random_<<5;
    int selected=random_%(previousColour_<0?10:9);
    if(previousColour_>=0 && selected>=previousColour_) ++selected;
    previousColour_=selected;
    return palette[selected];
  }
};
