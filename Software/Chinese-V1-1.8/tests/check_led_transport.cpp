#include <cstdint>
#include <cassert>
#include <iostream>
#define HAPTIC_TRANSPORT_HOST_TEST
enum { RGB, NEO_GRB=32, NEO_KHZ800=0 };
struct TestRGB { uint8_t r,g,b; };
template<int Order> struct PixelController {
  TestRGB input[20]; int index=0;
  void preStepFirstByteDithering() {}
  bool has(int n) { return index+n<=20; }
  uint8_t loadAndScale0(){return input[index].r;}
  uint8_t loadAndScale1(){return input[index].g;}
  uint8_t loadAndScale2(){return input[index].b;}
  void stepDithering(){}
  void advanceData(){++index;}
};
template<int Order> struct CPixelLEDController {
  virtual void init()=0;
  virtual void showPixels(PixelController<Order>&)=0;
};
bool beginResult=true;int shows=0;TestRGB sent[20];
struct Adafruit_NeoPixel {
  Adafruit_NeoPixel(int count,int pin,int flags) {assert(count==20 && pin==15 && flags==32);}
  bool begin(){return beginResult;}
  void setPixelColor(int index,uint8_t r,uint8_t g,uint8_t b){sent[index]={r,g,b};}
  void show(){++shows;}
};
#include "../firmware/MacroPad/neopixel_transport.h"
struct ExposedTransport:NeoPixelTransport<20,15> { using NeoPixelTransport::showPixels; };
int main(){
  ExposedTransport output;PixelController<RGB> pixels;
  for(int i=0;i<20;++i)pixels.input[i]={uint8_t(i),uint8_t(i+50),uint8_t(i+100)};
  output.showPixels(pixels);assert(shows==0);
  output.init();assert(output.ready());output.showPixels(pixels);assert(shows==1 && pixels.index==20);
  for(int i=0;i<20;++i)assert(sent[i].r==i && sent[i].g==i+50 && sent[i].b==i+100);
  beginResult=false;output.init();pixels.index=0;output.showPixels(pixels);assert(!output.ready() && shows==1);
  std::cout<<"PASS: production transport preserves 20 scaled RGB pixels, pin/order and initialization failure handling\n";
}
