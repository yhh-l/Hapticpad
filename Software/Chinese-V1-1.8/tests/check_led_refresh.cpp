#include <cstdint>
#include <cstring>
#include <cassert>
#include <cmath>
#include <iostream>
#include <atomic>
#include "../firmware/MacroPad/led_follow.h"
#include "../firmware/MacroPad/led_trail.h"
using byte = uint8_t;
constexpr int NUM_LEDS = 20;
struct RGB { byte red = 0, green = 0, blue = 0; };
using CRGB = RGB;
RGB leds[NUM_LEDS];
RGB CHSV(unsigned h, unsigned s, unsigned v) { return {byte(h),byte(s),byte(v)}; }
struct {
  unsigned shows = 0;
  int brightness = 0, sentBrightness = -1;
  RGB sent[NUM_LEDS];
  void setBrightness(int value) { brightness = value; }
  void show() { ++shows; sentBrightness = brightness; memcpy(sent,leds,sizeof leds); }
} FastLED;
uint32_t nowMs=0;
unsigned millis() { return nowMs; }
struct { uint32_t hwrand32() { return 123; } } rp2040;
std::atomic<unsigned> generation{0};
byte primaryColour[3] = {0,48,120}, secondaryColour[3] = {};
int ledBrightness=35, ledSpeed=50, sequenceStep=0, ledMode=4, haloCount=0, loopCounter=0;
float redScale=0, greenScale=0, blueScale=0;
bool breathIncrease=true, evenNumber=false;
std::atomic<float> rawTelemetry{0};
struct { int ledDirection=1, ledOffset=0; } profile;
#ifndef LED_SOURCE
#define LED_SOURCE "../firmware/MacroPad/LED.ino"
#endif
#include LED_SOURCE
void expectSolid() {
  assert(FastLED.sentBrightness==ledBrightness);
  for(auto c:FastLED.sent) assert(c.red==primaryColour[0] && c.green==primaryColour[1] && c.blue==primaryColour[2]);
}
int main() {
  solidLED(); assert(FastLED.shows==1); expectSolid();
  for(int i=0;i<1000;++i) solidLED();
  assert(FastLED.shows==1); // Regression: identical Solid frames must stay latched.
  primaryColour[1]=120; solidLED(); assert(FastLED.shows==2); expectSolid();
  ledBrightness=60; solidLED(); assert(FastLED.shows==3); expectSolid();
  offLED(); assert(FastLED.shows==4);
  for(auto c:FastLED.sent) assert(c.red==0 && c.green==0 && c.blue==0);
  for(int i=0;i<100;++i) offLED();
  assert(FastLED.shows==4);
  solidLED(); assert(FastLED.shows==5); expectSolid();
  // Animated renderers must continue delivering frames even if some consecutive
  // frames happen to be identical; returning to Solid restores its actual RGB.
  for(auto effect:{haloLED,breathLED,ledBand,rainbowLED}) {
    unsigned before=FastLED.shows;
    for(int i=0;i<3;++i) effect();
    assert(FastLED.shows==before+3);
    solidLED(); expectSolid();
    unsigned held=FastLED.shows;
    for(int i=0;i<10;++i) solidLED();
    assert(FastLED.shows==held);
  }
  primaryColour[0]=primaryColour[1]=primaryColour[2]=0;
  ledBrightness=0; solidLED(); expectSolid();
  ledBrightness=255; primaryColour[2]=255; solidLED(); expectSolid();
  for(int mode=0;mode<6;++mode) {
    ledMode=mode; calculateColourMultiplier();
    assert(std::isfinite(redScale)&&std::isfinite(greenScale)&&std::isfinite(blueScale));
  }
  primaryColour[0]=0; primaryColour[1]=120; primaryColour[2]=255;
  secondaryColour[0]=0; secondaryColour[1]=4; secondaryColour[2]=12;
  constexpr float sector=6.283185307179586f/20;
  for(int step=0;step<=40;++step) {
    rawTelemetry.store(step*sector); knobLED();
    assert(ledPixel==step%20);
    for(int i=0;i<NUM_LEDS;++i) {
      auto c=FastLED.sent[i]; const byte *expected=i==ledPixel?primaryColour:secondaryColour;
      assert(c.red==expected[0] && c.green==expected[1] && c.blue==expected[2]);
    }
  }
  unsigned held=FastLED.shows;
  for(int i=0;i<1000;++i) knobLED();
  assert(FastLED.shows==held);
  profile.ledDirection=-1; profile.ledOffset=5;
  knobLED(); assert(ledPixel==5);
  secondaryColour[2]=15; knobLED(); assert(FastLED.shows==held+2);
  solidLED(); expectSolid();
  std::cout << "PASS: static hold, color/brightness changes, off/on, animation transitions, multiplier initialization\n";
  std::cout << "PASS: one tracked highlight, dark background, two complete turns, idle hold, direction/offset/color changes\n";
  profile.ledDirection=1;profile.ledOffset=0;rawTelemetry.store(0);
  auto expectBackground=[](){ for(auto c:FastLED.sent) assert(c.red==secondaryColour[0]&&c.green==secondaryColour[1]&&c.blue==secondaryColour[2]); };
  knobTrailLED(); expectBackground();
  unsigned idle=FastLED.shows;
  nowMs=100;knobTrailLED();assert(FastLED.shows==idle);
  rawTelemetry.store(.1f);knobTrailLED();assert(FastLED.shows==idle+1);
  auto first=FastLED.sent[0];
  nowMs=150;rawTelemetry.store(sector);knobTrailLED();
  auto second=FastLED.sent[1];assert(first.red!=second.red||first.green!=second.green||first.blue!=second.blue);
  nowMs=1150;knobTrailLED();auto halfway=FastLED.sent[0];
  assert(halfway.red==(first.red+secondaryColour[0])/2);
  nowMs=2150;knobTrailLED();expectBackground();
  idle=FastLED.shows;nowMs=5000;knobTrailLED();assert(FastLED.shows==idle);
  generation.fetch_add(1);knobTrailLED();expectBackground();
  std::cout<<"PASS: trail renderer, random per sector, exact 2s return to background and no idle transfers\n";
}
