#include <cassert>
#include <cmath>
#include <iostream>
#include "haptics.h"

float armAndStart(AssistPulse &pulse, FeedbackSettings &s, float velocity, uint32_t start=0) {
  assert(pulse.update(s,0,start,true)==0);
  assert(pulse.update(s,0,start+250,true)==0);
  assert(pulse.update(s,velocity,start+251,true)==0);
  assert(pulse.update(s,velocity,start+291,true)==0);
  return pulse.update(s,velocity,start+326,true);
}
int main(){
  FeedbackSettings s; s.mode=ASSIST;s.assist=100;s.voltage=.6f;s.damping=50;
  for(float velocity:{-100.f,-2.f,0.f,2.f,100.f}){
    float q=dampingVoltage(s,velocity);
    assert(q*velocity<=0);assert(fabsf(q)<=s.voltage);
  }
  assert(boundedVoltage(NAN,1)==0);assert(boundedVoltage(INFINITY,1)==0);
  assert(boundedVoltage(5,20)==3);assert(boundedVoltage(-5,20)==-3);
  AssistPulse p;
  for(unsigned t=0;t<1000;++t)assert(p.update(s,2,t,true)==0); // no rest, no powered startup
  p.reset();float q=armAndStart(p,s,2);assert(q>0&&q<=.6f);
  for(uint32_t t=327;t<541;++t){float out=p.update(s,2,t,true);assert(out>=0&&out<=.6f);}
  for(uint32_t t=541;t<2000;++t)assert(p.update(s,2,t,true)==0); // timeout latches until new rest
  assert(p.update(s,0,2000,true)==0);assert(p.update(s,0,2250,true)==0);
  assert(p.update(s,-2,2251,true)==0);assert(p.update(s,-2,2291,true)==0);
  assert(p.update(s,-2,2326,true)<0);assert(p.update(s,2,2327,true)==0); // reverse cuts force
  p.reset();assert(armAndStart(p,s,2)>0);
  assert(p.update(s,s.speedRPM*.104719755f,327,true)==0);
  assert(p.update(s,2,400,true)==0); // overspeed also latches
  p.reset();assert(armAndStart(p,s,2)>0);assert(p.update(s,2,327,false)==0);
  assert(p.update(s,2,400,true)==0); // disable cannot restart an existing spin
  p.reset();assert(armAndStart(p,s,-2,UINT32_MAX-200)<0); // millis wraps correctly
  p.reset();s.voltage=.1f;assert(armAndStart(p,s,2)<=.1f);
  std::cout<<"Haptics: passive damping, voltage bounds, signed assist, rest/rearm, timeout, reverse, overspeed, disable, timer wrap passed\n";
}
