// WS2812 pixels latch their last frame. Static modes need a new transfer only
// when RGB or brightness changes. Animated modes still send at the usual rate.
CRGB lastLedFrame[NUM_LEDS];
int lastLedBrightness = -1;
bool ledFrameValid = false;
uint32_t ledFrames = 0;
int ledPixel = -1;

void showLEDFrame(bool animated) {
  if (!animated && ledFrameValid && lastLedBrightness == ledBrightness &&
      memcmp(lastLedFrame, leds, sizeof(leds)) == 0) return;
  FastLED.setBrightness(ledBrightness);
  FastLED.show();
  memcpy(lastLedFrame, leds, sizeof(leds));
  lastLedBrightness = ledBrightness;
  ledFrameValid = true;
  ++ledFrames;
}

void ledBand(){
  if(loopCounter == 20){
    if(evenNumber){
      evenNumber = false;
    } else {
      evenNumber = true;
    }
    loopCounter = 0;
  }

  for(int i = 0; i < NUM_LEDS; i++){
    if(evenNumber){
      if(i % 2){
        leds[i].red = primaryColour[0] - (redScale * sequenceStep);
        leds[i].green = primaryColour[1] - (greenScale * sequenceStep);
        leds[i].blue = primaryColour[2] - (blueScale * sequenceStep);
      } else {
        leds[i].red = secondaryColour[0] - (redScale * sequenceStep);
        leds[i].green = secondaryColour[1] - (greenScale * sequenceStep);
        leds[i].blue = secondaryColour[2] - (blueScale * sequenceStep);
      }
    } else {
      if(i % 2){
        leds[i].red = secondaryColour[0] - (redScale * sequenceStep);
        leds[i].green = secondaryColour[1] - (greenScale * sequenceStep);
        leds[i].blue = secondaryColour[2] - (blueScale * sequenceStep);
      } else {
        leds[i].red = primaryColour[0] - (redScale * sequenceStep);
        leds[i].green = primaryColour[1] - (greenScale * sequenceStep);
        leds[i].blue = primaryColour[2] - (blueScale * sequenceStep);
      }
    }
  }
  showLEDFrame(true);
  loopCounter++;
}

void breathLED(){
  if(breathIncrease){
    if(sequenceStep < 100){
      sequenceStep++;
    } else {
      breathIncrease = false;
    }
  } else {
    if(sequenceStep > 0){
      sequenceStep--;
    } else {
      breathIncrease = true;
    }
  }
  for(int i = 0; i < NUM_LEDS; i++){
    leds[i].red = primaryColour[0] - (redScale * sequenceStep);
    leds[i].green = primaryColour[1] - (greenScale * sequenceStep);
    leds[i].blue = primaryColour[2] - (blueScale * sequenceStep);
    //colors[i] = rgb_color(primaryColour[0] - (redScale * sequenceStep),primaryColour[1] - (greenScale * sequenceStep),primaryColour[2] - (blueScale * sequenceStep));
  }
  showLEDFrame(true);
  //ledStrip.write(colors, ledCount, ledBrightness);
}

void haloLED(){
  for(int i = 0; i < NUM_LEDS; i++){
    if(i == haloCount){
      sequenceStep = 0;
    }
    leds[i].red = primaryColour[0] - (redScale * sequenceStep);
    leds[i].green = primaryColour[1] - (greenScale * sequenceStep);
    leds[i].blue = primaryColour[2] - (blueScale * sequenceStep);
    sequenceStep++;
  }

  if(haloCount < NUM_LEDS){
    haloCount++;
  } else {
    haloCount = 0;
  }

  showLEDFrame(true);
}

void solidLED(){
  for(int i = 0; i < NUM_LEDS; i++){
    leds[i].red = primaryColour[0];
    leds[i].green = primaryColour[1];
    leds[i].blue = primaryColour[2];
  }

  showLEDFrame(false);
}

void offLED(){
  for (int i = 0; i < NUM_LEDS; ++i) leds[i] = CRGB{};
  showLEDFrame(false);
}

void rainbowLED(){
  uint8_t time = millis() >> 4;

  for(uint16_t i = 0; i < NUM_LEDS; i++)
  {
    uint8_t p = time - i * ledSpeed;
    leds[i] = CHSV((uint32_t)p * 359 / 256, 255, 255);
  }

  showLEDFrame(true);
}

void knobLED() {
  static KnobPixelTracker tracker;
  ledPixel = tracker.update(rawTelemetry.load(), NUM_LEDS, profile.ledDirection, profile.ledOffset);
  for (int i = 0; i < NUM_LEDS; ++i) {
    const byte *color = i == ledPixel ? primaryColour : secondaryColour;
    leds[i].red = color[0]; leds[i].green = color[1]; leds[i].blue = color[2];
  }
  showLEDFrame(false); // Send on sector/color/brightness changes; hold at rest.
}

void knobTrailLED() {
  static KnobPixelTracker tracker;
  static KnobColourTrail<NUM_LEDS> trail;
  static unsigned seenGeneration = ~0u;
  unsigned currentGeneration = generation.load();
  if (seenGeneration != currentGeneration) {
    trail.reset(rp2040.hwrand32()); seenGeneration = currentGeneration;
  }
  float angle = rawTelemetry.load();
  ledPixel = tracker.update(angle, NUM_LEDS, profile.ledDirection, profile.ledOffset);
  uint32_t now = millis();
  trail.update(ledPixel, trail.moved(angle), now);
  TrailRGB background{secondaryColour[0], secondaryColour[1], secondaryColour[2]};
  for (int i = 0; i < NUM_LEDS; ++i) {
    auto color = trail.pixel(i, now, background);
    leds[i].red = color.r; leds[i].green = color.g; leds[i].blue = color.b;
  }
  showLEDFrame(false); // Fade only while pixels change; stop sending at rest.
}

void calculateColourMultiplier(){
  int steps = 1; // Non-gradient modes do not use these multipliers.
  if(ledMode == 0){
    steps = 20;
  } else if(ledMode == 1){
    steps = 100;
  } else if(ledMode == 2){
    steps = 10;
  }
  redScale = primaryColour[0] - secondaryColour[0];
  redScale = redScale / steps; //Result 1.5

  greenScale = primaryColour[1] - secondaryColour[1];
  greenScale = greenScale / steps; //Result 0

  blueScale = primaryColour[2] - secondaryColour[2];
  blueScale = blueScale / steps; //Result -5
}