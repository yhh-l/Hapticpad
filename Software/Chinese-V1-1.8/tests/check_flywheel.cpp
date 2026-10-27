#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include "../firmware/MacroPad/flywheel.h"
static float rpm(float value){return value*.104719755f;}
void rest(Flywheel &f,const FlywheelSettings &s,uint32_t at=0){for(unsigned t=0;t<=300;t+=10)assert(f.update(s,0,at+t,true)==0);assert(f.armed());}
float launch(Flywheel &f,const FlywheelSettings &s,float velocity,uint32_t at=0){
  rest(f,s,at);float result=0;
  for(unsigned t=310;t<=400;t+=10)result=f.update(s,velocity,at+t,true);
  assert(f.active());assert(result*velocity>0);return result;
}
int main(){
  FlywheelSettings s;Flywheel f;
  for(unsigned t=0;t<2000;t+=10)assert(f.update(s,rpm(60),t,true)==0); // No rest: no powered startup.
  f.reset();float target=launch(f,s,rpm(60));assert(target<=rpm(s.maxRPM));
  float previous=target;
  for(unsigned t=410;t<20370;t+=10){
    target=f.update(s,std::fmax(previous,.3f),t,true);
    assert(target<=previous+.0001f && target>=0);previous=target;
  }
  assert(target==0&&!f.active()); // Powered feedback never extends the one-shot duration.
  for(unsigned t=20370;t<24000;t+=10)assert(f.update(s,rpm(30),t,true)==0);
  f.reset();target=launch(f,s,-rpm(60));assert(target<0);
  assert(f.update(s,rpm(10),410,true)==0&&!f.active()); // Reverse cuts assistance.
  for(unsigned t=420;t<1000;t+=10)assert(f.update(s,-rpm(60),t,true)==0);
  f.reset();target=launch(f,s,rpm(60));
  for(unsigned t=410;t<=510;t+=10)target=f.update(s,rpm(10),t,true);
  assert(target==0&&!f.active()); // Holding/braking cuts within configured time.
  f.reset();launch(f,s,rpm(60));assert(f.update(s,0,410,true)==0);
  f.reset();launch(f,s,rpm(60));assert(f.update(s,rpm(60),410,false)==0);
  assert(f.update(s,rpm(60),420,true)==0);
  f.reset();launch(f,s,rpm(60));assert(f.update(s,rpm(60),800,true)==0); // Loop stall.
  f.reset();launch(f,s,rpm(60));assert(f.update(s,NAN,410,true)==0);
  f.reset();launch(f,s,rpm(60));assert(f.update(s,INFINITY,410,true)==0);
  f.reset();launch(f,s,rpm(60));assert(f.update(s,rpm(s.maxRPM*2),410,true)==0);
  f.reset();launch(f,s,rpm(60),UINT32_MAX-350);assert(f.update(s,rpm(60),70,true)>0); // Timer wrap.
  f.reset();rest(f,s);for(unsigned t=310;t<1000;t+=10)assert(f.update(s,.1f*std::sin(t),t,true)==0); // Noise.
  f.reset();s.voltage=0;for(unsigned t=0;t<1000;t+=10)assert(f.update(s,rpm(60),t,true)==0);

  // Three fresh encoder readings: discard first, average only second/third.
  for(int direction:{-1,1}) {
    FlywheelSettings cfg;cfg.boostPercent=130;cfg.captureMs=120;Flywheel sample;rest(sample,cfg);
    assert(sample.update(cfg,-direction*rpm(300),310,true)==0);
    assert(sample.update(cfg,direction*rpm(30),312,true)==0);
    float chosen=sample.update(cfg,direction*rpm(90),314,true);
    assert(sample.active()&&std::fabs(chosen-direction*rpm(78))<.0001f);
    assert(sample.sampleCount()==3);
    float next=sample.update(cfg,direction*rpm(110),316,true);
    assert(std::fabs(next)<=std::fabs(chosen)); // No motor-driven recapture.
  }
  // If the next poll has no fresh encoder reading, two readings use #2.
  for(int direction:{-1,1}) {
    FlywheelSettings cfg;cfg.boostPercent=130;Flywheel two;rest(two,cfg);
    assert(two.update(cfg,direction*rpm(300),310,true)==0);
    assert(two.update(cfg,direction*rpm(40),312,true)==0);
    float selected=two.update(cfg,direction*rpm(40),314,true,0,false);
    assert(two.active()&&two.sampleCount()==2);
    assert(std::fabs(selected-direction*rpm(52))<.0001f);
    Flywheel one;rest(one,cfg);
    one.update(cfg,direction*rpm(300),310,true);
    assert(one.update(cfg,direction*rpm(40),312,true,0,false)==0&&!one.active());
    assert(one.update(cfg,0,314,true,0,false)==0&&!one.active());
  }
  // A user may stop and flick again without waiting a quarter second.
  {
    FlywheelSettings cfg;cfg.captureMs=20;Flywheel rapid;
    launch(rapid,cfg,rpm(60));rapid.update(cfg,0,410,true);
    for(uint32_t t=420;t<=550;t+=10) rapid.update(cfg,0,t,true);
    assert(rapid.armed());
  }
  // Sub-count speed noise must never launch a motor.
  {
    FlywheelSettings cfg;cfg.captureMs=20;cfg.triggerRPM=3;Flywheel noise;rest(noise,cfg);
    for(uint32_t t=310;t<1000;t+=2) {
      noise.update(cfg,t%40<4?.45f:0.f,t,true);
      assert(!noise.active());
    }
  }
  // Boost once from the unpowered hand peak; never compound motor motion.
  for(int direction:{-1,1}) {
    FlywheelSettings cfg;cfg.captureMs=20;cfg.boostPercent=130;
    Flywheel boost;float target=launch(boost,cfg,direction*rpm(60));
    assert(std::fabs(target)>rpm(75)&&std::fabs(target)<rpm(79));
    for(uint32_t t=410;t<700;t+=10) {
      float next=boost.update(cfg,direction*rpm(60),t,true,.7f);
      assert(next!=0&&std::fabs(next)<=std::fabs(target));target=next;
    }
    assert(!boost.loadStopped()); // The requested boost itself is not hand braking.
    for(uint32_t t=710;t<=760;t+=10) boost.update(cfg,direction*rpm(30),t,true,.7f);
    assert(boost.loadStopped()&&!boost.active());
  }
  {FlywheelSettings quick;quick.captureMs=20;Flywheel fast;
   float target=launch(fast,quick,rpm(300));assert(target>0&&target<=rpm(quick.maxRPM));
   assert(fast.update(quick,rpm(300),410,true)>0); // Manual peak above motor target is allowed; never commanded faster than cap.
  }
  {FlywheelSettings cfg;Flywheel plateau;rest(plateau,cfg);
   for(uint32_t t=310;t<=400;t+=10)plateau.update(cfg,rpm(60+(t-310)*.00001f),t,true);
   assert(plateau.active()); // Quantization/LPF residue must not force the full 120ms capture wait.
  }
  // Estimated load requires both a sustained speed deficit and real forward
  // drive. High drive alone or fast manual rotation must not count as touch.
  for(int direction:{-1,1}) {
    FlywheelSettings cfg;Flywheel load;launch(load,cfg,direction*rpm(60));
    for(uint32_t t=410;t<=500;t+=10)assert(load.update(cfg,direction*rpm(60),t,true,.9f)!=0);
    for(uint32_t t=510;t<=550;t+=10)load.update(cfg,direction*rpm(40),t,true,.7f);
    assert(!load.active()&&load.loadStopped());
    assert(load.update(cfg,direction*rpm(30),560,true,.7f)==0); // No automatic restart against the hand.
    load.reset();launch(load,cfg,direction*rpm(60));
    for(uint32_t t=410;t<=560;t+=10)assert(load.update(cfg,direction*rpm(45),t,true,0)!=0);
    assert(!load.loadStopped()); // Passive deceleration without motor drive is not a load trigger.
  }
  for(int direction:{-1,1})for(float q:{-5.f,-.2f,0.f,.2f,5.f}) {
    float out=flywheelDriveVoltage(q,direction,.8f,1.f);
    assert(out*direction>=0 && std::fabs(out)<=.8f);
  }
  assert(flywheelDriveVoltage(1,1,.8f,0)==0);
  assert(flywheelDriveVoltage(NAN,1,.8f,1)==0);
  std::cout<<"PASS: rest/rearm, signed one-shot 20s coast, monotonic decay without self-retrigger, braking/reverse/stop/disable/overspeed, stale/invalid samples, timer wrap, no reverse torque and voltage bounds\n";
}
