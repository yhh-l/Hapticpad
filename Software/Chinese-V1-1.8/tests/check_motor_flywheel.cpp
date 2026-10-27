#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include "../firmware/MacroPad/device.h"
constexpr float PI=3.14159265358979323846f;
template<class T>T constrain(T x,T lo,T hi){return x<lo?lo:x>hi?hi:x;}
uint32_t clockUs=0;
uint32_t micros(){return clockUs;}
uint32_t millis(){return clockUs/1000;}
struct {float angle=0;void update(){}float getAngle(){return angle;}} encoder;
std::atomic<bool> deviceStandby{false};
std::atomic<bool> configBusy{false},configAcknowledged{false},motorPaused{true},menuOpen{false},wantMotor{true},focReady{true};
std::atomic<float> flywheelMeasuredRPM{0},flywheelPeakRPM{0};
std::atomic<bool> flywheelArmed{false};
std::atomic<bool> flywheelLoadStop{false};
std::atomic<unsigned> generation{0},flywheelState{0},flywheelSamples{0};
std::atomic<uint32_t> flywheelRemaining{0};
std::atomic<float> rawTelemetry{0},outputTelemetry{0},shaftTelemetry{0},targetTelemetry{0};
std::atomic<int> sensorSign{1},wheelEvents{0};
Profile profile;
float Clicky_P=.5f,Clicky_I=0,Twist_P=.65f,Twist_I=.2f,Momentum_P=.3f,Momentum_I=0;
enum class MotionControlType {angle,velocity,torque};
float measuredVelocity=0;
struct {
 bool enabled=false;unsigned enables=0,disables=0,moves=0,loops=0,calibrations=0;
 struct {float P=0,I=0,limit=0;void reset(){}}PID_velocity;
 struct {float limit=0;}P_angle;
 struct {float Tf=0;}LPF_velocity;
 struct {float q=0,d=0;}voltage;
 float voltage_limit=0,voltage_sensor_align=0,velocity_limit=0,sensor_offset=.2f;
 int sensor_direction=1;MotionControlType controller=MotionControlType::angle;
 void enable(){enabled=true;++enables;}void disable(){enabled=false;voltage.q=0;++disables;}
 bool initFOC(){++calibrations;return true;}
 void move(float target){++moves;voltage.q=PID_velocity.P*(target-measuredVelocity*sensorSign.load());}
 void loopFOC(){++loops;}
} motor;
#include "../firmware/MacroPad/wheel.ino"
void step(float velocity,int ticks=1){measuredVelocity=velocity;for(int i=0;i<ticks;++i){clockUs+=2000;encoder.angle+=velocity*.002f;motorTick();}}
void reset(int sign=1){wantMotor.store(true);configBusy.store(false);menuOpen.store(false);focReady.store(true);sensorSign.store(sign);motor.sensor_direction=sign;profile=Profile{};profile.mode=3;generation.fetch_add(1);step(0,160);assert(!motor.enabled&&outputTelemetry.load()==0);}
void launch(float velocity){step(velocity,80);assert(flywheelState.load()==Flywheel::COAST);assert(!motor.enabled);step(velocity*.9f,10);assert(motor.enabled);assert(flywheelRemaining.load()>0);}
int main(){
 reset();launch(4);assert(motor.enabled);deviceStandby.store(true);step(1,20);
 assert(!motor.enabled && motorPaused.load() && outputTelemetry.load()==0 && flywheelState.load()==Flywheel::IDLE);
 wheelEvents.store(0);step(3,50);assert(wheelEvents.load()==0);
 deviceStandby.store(false);generation.fetch_add(1);step(0,160);assert(!motor.enabled);
 // Actual angle deltas, rather than start-up LPF values, feed capture.
 for(int sign:{-1,1})for(int direction:{-1,1}) {
  reset(sign);profile.flywheel.boostPercent=130;generation.fetch_add(1);step(0,160);
  unsigned before=motor.enables;
  step(-direction*1.f);step(direction*4.f);assert(!motor.enabled&&motor.enables==before);
  step(direction*8.f);
  assert(flywheelState.load()==Flywheel::COAST&&flywheelSamples.load()==3);
  assert(std::fabs(flywheelPeakRPM.load()-6.f*9.549296586f)<.02f);
  assert(std::fabs(targetTelemetry.load()-sign*direction*7.8f)<.002f);
 }
 reset();unsigned idleMoves=motor.moves;step(0,10);assert(motor.moves>idleMoves&&!motor.enabled); // Keep encoder velocity history fresh while unpowered.
 reset();unsigned captureEnables=motor.enables;step(4,1);assert(flywheelState.load()==Flywheel::CAPTURE&&!motor.enabled&&motor.enables==captureEnables);
 reset();unsigned enabled=motor.enables;step(0,500);assert(motor.enables==enabled); // No driver pulses at rest.
 for(int sign:{-1,1})for(int direction:{-1,1}){
  reset(sign);launch(direction*4.f);
  assert(targetTelemetry.load()*sign*direction>0);
  step(direction*3.5f,10);assert(outputTelemetry.load()*sign*direction>=0);
  unsigned events=wheelEvents.exchange(0);assert(events!=0);
  step(0,200);assert(!motor.enabled&&outputTelemetry.load()==0&&flywheelState.load()==Flywheel::IDLE);
  wheelEvents.store(0);step(0,300);assert(wheelEvents.load()==0); // No virtual wheel events when angle is stationary.
 }
 Momentum_P=.9f;reset();profile.feedback.strength=60;generation.fetch_add(1);step(0,160);launch(4);step(4,60);step(2.8f,40);assert(!motor.enabled&&flywheelLoadStop.load()); // Estimated hand load cuts actual driver output.
 step(2.8f,100);assert(!motor.enabled);Momentum_P=.3f; // No restart until rest.
 reset();launch(4);step(8,20);assert(outputTelemetry.load()==0&&!motor.enabled); // Faster manual motion is never electrically braked.
 step(-2,100);assert(!motor.enabled&&flywheelState.load()==Flywheel::IDLE);
 reset();launch(4);wantMotor.store(false);step(4);assert(!motor.enabled&&flywheelRemaining.load()==0);
 wantMotor.store(true);step(4,100);assert(!motor.enabled); // Reenable while spinning cannot relaunch.
 reset();launch(4);generation.fetch_add(1);step(4,100);assert(!motor.enabled&&flywheelRemaining.load()==0);
 reset();launch(4);configBusy.store(true);step(4);assert(!motor.enabled&&configAcknowledged.load()&&flywheelState.load()==0);
 configBusy.store(false);step(4,100);assert(!motor.enabled);
 reset();launch(4);menuOpen.store(true);generation.fetch_add(1);step(0);assert(flywheelState.load()==0&&motor.controller==MotionControlType::angle);
 menuOpen.store(false);generation.fetch_add(1);step(4,100);assert(!motor.enabled);
 reset();focReady.store(false);unsigned count=motor.calibrations;step(0);assert(motor.calibrations==count+1&&!motor.enabled);step(0,160);assert(motor.calibrations==count+1);
 reset();launch(4);clockUs+=200000;step(4);assert(!motor.enabled&&flywheelState.load()==0);
 std::cout<<"PASS: production motorTick idle/capture drive-off, four direction mappings, physical HID only, no braking torque at faster speed, reverse/stop/disable/menu/config/stale cancellation, one-shot FOC calibration\n";
}
