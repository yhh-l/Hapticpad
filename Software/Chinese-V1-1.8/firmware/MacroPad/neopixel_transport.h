#pragma once
#ifndef HAPTIC_TRANSPORT_HOST_TEST
#include <FastLED.h>
#include <Adafruit_NeoPixel.h>
#endif

// Keep FastLED's existing RGB/brightness pipeline; replace only the physical
// RP2040 transport. Adafruit's PIO backend feeds its FIFO synchronously,
// without the FastLED DMA channel/IRQ path. No GPIO drive settings are changed.
template<int Count,int Pin> class NeoPixelTransport : public CPixelLEDController<RGB> {
 public:
  void init() override { ready_=strip_.begin(); }
  bool ready() const { return ready_; }
 protected:
  void showPixels(PixelController<RGB> &pixels) override {
    if(!ready_) return;
    pixels.preStepFirstByteDithering();
    int i=0;
    while(pixels.has(1) && i<Count) {
      strip_.setPixelColor(i++,pixels.loadAndScale0(),pixels.loadAndScale1(),pixels.loadAndScale2());
      pixels.stepDithering(); pixels.advanceData();
    }
    strip_.show();
  }
 private:
  Adafruit_NeoPixel strip_{Count,Pin,NEO_GRB+NEO_KHZ800};
  bool ready_=false;
};
