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

void drawFeiyuKeyLabels() {
  u8g2.drawHLine(0,16,128);
  u8g2.drawHLine(0,47,128);
  for(int x : {42,85}) {
    u8g2.drawVLine(x,0,16);
    u8g2.drawVLine(x,48,16);
  }
  for(int i=0;i<6;++i) {
    int x=i%3*43, y=i<3?0:48;
    u8g2.setFont(u8g2_font_5x7_tf);
    char number[2]={(char)('1'+i),0};
    u8g2.drawStr(x+1,y+12,number);
    u8g2.setFont(u8g2_font_wqy12_t_gb2312);
    clipped(x+8,y+12,profile.labels[i],33);
  }
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
  drawFeiyuKeyLabels();
  if(drawFeiyuNotice()) return;
  drawFeiyuCharacter(255);
}
