#pragma once
#include <cmath>
#include <cstdint>

struct FlywheelSettings {
  uint32_t durationMs=20000,halfLifeMs=6000;
  uint16_t triggerRPM=8,maxRPM=120,brakePercent=55,brakeMs=80,captureMs=40,boostPercent=100;
  float voltage=.8f;
};

// Only assist in the captured direction. Exceeding the decaying target coasts
// with zero drive, rather than electrically braking a faster manual spin.
inline float flywheelDriveVoltage(float q,int direction,float cap,float envelope) {
  if(!std::isfinite(q)||!std::isfinite(cap)||!std::isfinite(envelope)||cap<=0||envelope<=0)return 0;
  cap=std::fmin(cap,.9f)*std::fmin(envelope,1.f);
  if(direction!=1&&direction!=-1)return 0;
  float drive=q*direction;
  if(drive<0)return 0;
  return direction*std::fmin(drive,cap);
}

// Encoder-only motor assistance cannot distinguish a fresh human push from
// motion caused by its own motor. Capture once while unpowered, then decay a
// stored target on the clock. It may only rearm after a new stationary period.
class Flywheel {
 public:
  enum State:uint8_t {IDLE,CAPTURE,COAST};
  void reset(){state_=IDLE;sampleCount_=0;armed_=resting_=braking_=clock_=loading_=loadStop_=false;target_=envelope_=0;}
  bool active() const {return state_==COAST;}
  bool armed() const {return armed_;}
  uint8_t state() const {return state_;}
  float envelope() const {return envelope_;}
  uint32_t remaining(const FlywheelSettings &s,uint32_t now) const {
    if(!active())return 0;uint32_t elapsed=now-started_;
    return elapsed>=s.durationMs?0:s.durationMs-elapsed;
  }
  float peakRPM() const {return peak_*9.549296586f;}
  uint8_t sampleCount() const {return sampleCount_;}
  bool loadStopped() const {return loadStop_;}
  float update(const FlywheelSettings &s,float velocity,uint32_t now,bool allowed,float driveFraction=0.f,bool newReading=true) {

    if(!allowed||!std::isfinite(velocity)||!valid(s)){reset();return 0;}
    if(clock_) {
      if(uint32_t(now-lastAt_)>100){reset();lastAt_=now;clock_=true;return 0;}
      if(now==lastAt_)return target_;
    }
    clock_=true;lastAt_=now;
    const float speed=std::fabs(velocity),trigger=s.triggerRPM*.104719755f,limit=s.maxRPM*.104719755f;
    if(state_==IDLE) {
      target_=envelope_=0;
      if(speed<.2f) {
        if(!resting_){resting_=true;restAt_=now;}
        if(uint32_t(now-restAt_)>=120)armed_=true;
        return 0;
      }
      resting_=false;
      if(!armed_||speed<trigger){return 0;}
      if(!newReading)return 0;
      // Discard the first real encoder reading entirely, including its
      // direction. It must not enter the target or overspeed reference.
      direction_=0;state_=CAPTURE;sampleCount_=1;
      second_=peak_=referenceMax_=0;loadStop_=false;armed_=false;
      return 0;
    }
    if((sampleCount_>=2&&velocity*direction_<-.2f)||speed<.2f){reset();return 0;}
    if(state_==CAPTURE) {
      // No capture timer or peak/plateau wait: consume distinct encoder
      // readings as soon as they arrive. A repeated angle is not a reading.
      if(newReading) {
        if(sampleCount_==1){direction_=velocity>0?1:-1;second_=speed;sampleCount_=2;return 0;}
        peak_=(second_+speed)*.5f;referenceMax_=std::fmax(second_,speed);sampleCount_=3;
      } else {
        if(sampleCount_<2)return 0; // One reading never drives the motor.
        peak_=referenceMax_=second_; // Only two: use the last, never the first.
      }
      initial_=std::fmin(peak_*s.boostPercent/100.f,limit);started_=now;state_=COAST;braking_=false;
    }
    // A fast manual flick may exceed the powered target while unpowered.
    // Retain it as the reference for detecting a later unexplained overspeed.
    if(speed>std::fmax(limit,referenceMax_)*1.5f){reset();return 0;}
    uint32_t elapsed=now-started_;
    if(elapsed>=s.durationMs){reset();return 0;}
    float fade=elapsed>s.durationMs-1000?(s.durationMs-elapsed)/1000.f:1.f;
    float value=initial_*std::exp(-.69314718056f*elapsed/s.halfLifeMs)*fade;
    if(value<.15f){reset();return 0;}
    target_=direction_*value;
    envelope_=elapsed<12?elapsed/12.f:1.f;
    // Encoder/voltage proxy only, not a calibrated torque measurement. A
    // >20% speed deficit while applying >=25% of available forward drive for
    // 30ms withdraws assistance. Allow 80ms for initial takeover to settle.
    const float handReference=value*100.f/s.boostPercent;
    bool loaded=elapsed>=80 && speed<handReference*.8f && std::isfinite(driveFraction) && driveFraction>=.25f;
    if(loaded) {
      if(!loading_){loading_=true;loadAt_=now;}
      if(uint32_t(now-loadAt_)>=30){reset();loadStop_=true;return 0;}
    } else loading_=false;
    if(speed<handReference*s.brakePercent/100.f) {
      if(!braking_){braking_=true;brakeAt_=now;}
      if(uint32_t(now-brakeAt_)>=s.brakeMs){reset();return 0;}
    } else braking_=false;
    return target_;
  }
 private:
  static bool valid(const FlywheelSettings &s) {
    return s.durationMs>=3000&&s.durationMs<=30000&&s.halfLifeMs>=1000&&s.halfLifeMs<=12000&&
      s.triggerRPM>=3&&s.triggerRPM<=40&&s.maxRPM>=30&&s.maxRPM<=480&&s.triggerRPM<s.maxRPM&&
      s.boostPercent>=100&&s.boostPercent<=180&&
      s.brakePercent>=30&&s.brakePercent<=85&&s.brakeMs>=30&&s.brakeMs<=200&&s.captureMs>=20&&s.captureMs<=120&&
      std::isfinite(s.voltage)&&s.voltage>0&&s.voltage<=.9f;
  }
  State state_=IDLE;
  bool armed_=false,resting_=false,braking_=false,clock_=false,loading_=false,loadStop_=false;
  uint8_t sampleCount_=0;
  int direction_=0;
  uint32_t lastAt_=0,restAt_=0,started_=0,brakeAt_=0,loadAt_=0;
  float target_=0,initial_=0,peak_=0,referenceMax_=0,second_=0,envelope_=0;
};
