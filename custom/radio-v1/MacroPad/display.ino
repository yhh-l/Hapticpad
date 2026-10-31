uint32_t screenFrameMs=0, petQuoteId=0, petQuoteCount=0;
uint32_t feiyuScreenFrames=0, feiyuReactionAt=0;
uint8_t feiyuScreenPose=FEIYU_IDLE;

void clipped(int x, int y, const char *text, int width) {
  String copy=text;
  while(copy.length() && u8g2.getUTF8Width(copy.c_str())>width) {
    int n=copy.length()-1;
    while(n>0 && ((uint8_t)copy[n]&0xc0)==0x80) --n;
    copy.remove(n);
  }
  u8g2.drawUTF8(x,y,copy.c_str());
}

void drawFeiyuCharacter(uint8_t pose) {
  FeiyuScreenFrame frame=feiyuScreenFrame(screenFrameMs,idleTimer.elapsed(screenFrameMs),
      feiyuReactionAt && uint32_t(screenFrameMs-feiyuReactionAt)<1800);
  if(pose<FEIYU_SPRITE_COUNT) frame.pose=pose;
  feiyuScreenPose=frame.pose;
  drawFeiyuPixels([](int x,int y){u8g2.drawPixel(x,y);},frame);
}

void drawScreen() {
  ++feiyuScreenFrames;
  static float previousAngle=0;
  const float angle=rawTelemetry.load();
  if(buttonsMask || (std::isfinite(angle) && std::fabs(angle-previousAngle)>=.02f)) {
    feiyuReactionAt=screenFrameMs;
  }
  if(std::isfinite(angle)) previousAngle=angle;
  if(menuOpen.load()) {drawSettings();return;}
  if(drawFeiyuNotice()) return;
  drawFeiyuCharacter(255);
}
