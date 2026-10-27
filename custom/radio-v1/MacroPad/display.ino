uint32_t screenFrameMs = 0;
void clipped(int x, int y, const char *text, int width) {
  String copy = text;
  while (copy.length() && u8g2.getUTF8Width(copy.c_str()) > width) {
    int n = copy.length() - 1;
    while (n > 0 && ((uint8_t)copy[n] & 0xc0) == 0x80) --n;
    copy.remove(n);
  }
  u8g2.drawUTF8(x, y, copy.c_str());
}
// Short names are display-only; SD names and OS shortcut detection stay intact.
String displayName(const char *name) {
  String text = name;
  if (text.startsWith("Mac")) text = String("M") + text.substring(3);
  else if (text.startsWith("Win")) text = String("W") + text.substring(3);
  text.replace("刻度", "刻");
  text.replace("阻尼", "阻");
  text.replace("助转", "助");
  text.replace("无极续转", "无极");
  return text;
}
String screenDetail() {
  if (profile.displayText[0]) return String(profile.displayText);
  if (profile.feedback.mode == FREE) return "自由滚动";
  if (profile.feedback.mode == DAMPING) return String("阻力 ") + profile.feedback.damping + "%";
  if (profile.feedback.mode == ASSIST) return String("助力 ") + profile.feedback.assist + "%";
  if (profile.mode == 3) return String("续转倍率 ") + profile.flywheel.boostPercent + "%";
  if (profile.mode == 0) return String(profile.feedback.detents) + "格/圈 力" + profile.feedback.strength + "%";
  return String("反馈力度 ") + profile.feedback.strength + "%";
}
uint32_t petQuoteId = 0, petQuoteCount = 0;
String petSpeechPages[4];
uint32_t petSpeechDuration = 5000;
uint8_t petSpeechPageCount = 0;
uint32_t petSpeechAt = 0, petNextSpeech = 0;
int petSpeechProfile = -1;
bool petTalking = false;
uint32_t petRandomState = 1;
uint32_t nextPetRandom() {
  petRandomState ^= petRandomState << 13;
  petRandomState ^= petRandomState >> 17;
  petRandomState ^= petRandomState << 5;
  return petRandomState;
}
String indexedPetQuote(bool person) {
  struct QuoteFiles { FsFile text, index; ~QuoteFiles() { text.close(); index.close(); } };
  QuoteFiles files{SD.open(person ? "/person.tsv" : "/dog.tsv"), SD.open(person ? "/person.idx" : "/dog.idx")};
  FsFile &text=files.text, &index=files.index;
  uint32_t header[3] = {};
  petQuoteCount = 0;
  if (!text || !index || index.read(header,12)!=12 || header[0]!=0x31515048 ||
      !header[1] || header[1]>10000000 || header[2]!=text.size() ||
      index.size()!=12ULL+4ULL*(header[1]+1)) return "";
  petQuoteCount = header[1];
  uint32_t id;
  if (petQuoteCount>1 && petQuoteId>=1 && petQuoteId<=petQuoteCount) {
    id=1+nextPetRandom()%(petQuoteCount-1); if (id>=petQuoteId) ++id;
  } else id=1+nextPetRandom()%petQuoteCount;
  uint32_t offsets[2] = {};
  if (!index.seekSet(12+4ULL*(id-1)) || index.read(offsets,8)!=8 ||
      offsets[0]>=offsets[1] || offsets[1]>text.size() || offsets[1]-offsets[0]>144 ||
      !text.seekSet(offsets[0])) return "";
  char line[145] = {}; unsigned length = offsets[1]-offsets[0];
  if (text.read(line,length)!=int(length)) return "";
  String record = line; int tab = record.indexOf('\t');
  if (tab<0 || record.substring(0,tab).toInt()!=long(id)) return "";
  String quote=record.substring(tab+1); quote.trim();
  if (!quote.length() || quote.length()>120) return "";
  petQuoteId = id;
  return quote;
}
void updatePetSpeech(uint32_t now) {
  bool pet = profile.displayScene == 1 || profile.displayScene == 2;
  if (!pet || menuOpen.load()) { petTalking=false; petSpeechProfile=-1; return; }
  if (petSpeechProfile != activeProfile) {
    petSpeechProfile=activeProfile; petTalking=false; petNextSpeech=now+3500;
    petQuoteId=petQuoteCount=0;petRandomState=micros()|1;
  }
  if (petTalking && uint32_t(now-petSpeechAt)>=petSpeechDuration) {
    petTalking=false;petNextSpeech=now+6500;
  }
  if (petTalking || int32_t(now-petNextSpeech)<0) return;
  String quote=indexedPetQuote(profile.displayScene==2);
  if (!quote.length()) quote=profile.displayText[0] ? String(profile.displayText) :
      (profile.displayScene==2 ? String("一起散步吧") : String("汪，一起去玩吧"));
  for (auto &page : petSpeechPages) page="";
  petSpeechPageCount=1;
  u8g2.setFont(u8g2_font_wqy12_t_gb2312);
  for (unsigned i=0;i<quote.length();) {
    uint8_t lead=quote[i]; unsigned n=lead<0x80?1:(lead<0xe0?2:(lead<0xf0?3:4));
    if (i+n>quote.length()) break;
    String glyph=quote.substring(i,i+n), candidate=petSpeechPages[petSpeechPageCount-1]+glyph;
    if (u8g2.getUTF8Width(candidate.c_str())>120) {
      if (petSpeechPageCount==4) break;
      ++petSpeechPageCount;
    }
    petSpeechPages[petSpeechPageCount-1]+=glyph;i+=n;
  }
  petSpeechDuration = max(5000u, min(12000u, unsigned(quote.length()) * 100u));
  petTalking=true;petSpeechAt=now;
}
void drawScreen() {
  u8g2.setFont(u8g2_font_wqy12_t_gb2312);
  if (!sdDetected) {
    u8g2.drawUTF8(0, 20, "未读取到有效配置");
    u8g2.drawUTF8(0, 40, "请用网页写入配置"); return;
  }
  if (menuOpen.load()) { drawSettings(); return; }
  // Single-line key labels leave a 30px centre area for profile + custom text.
  u8g2.drawHLine(0, 16, 128); u8g2.drawHLine(0, 47, 128);
  for (int x : {42, 85}) { u8g2.drawVLine(x, 0, 16); u8g2.drawVLine(x, 48, 16); }
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.drawFrame(118, 19, 9, 11);
  u8g2.drawStr(120, 28, !wantMotor.load() || profile.feedback.mode == FREE || profile.feedback.voltage == 0 ? "F" : "N");
  u8g2.setFont(u8g2_font_wqy12_t_gb2312);
  if (radioHUD.supported(profile.name)) {
    u8g2.setFont(u8g2_font_wqy12_t_gb2312);
    clipped(2, 28, profile.name, 114);
    u8g2.setFont(u8g2_font_5x7_tf);
    if (radioHUD.fresh(screenFrameMs)) {
      char frequency[32], detail[64], stepText[12];
      if(radioHUD.step>=1000 && radioHUD.step%1000==0)snprintf(stepText,sizeof(stepText),"%luk",(unsigned long)(radioHUD.step/1000));
      else snprintf(stepText,sizeof(stepText),"%lu",(unsigned long)radioHUD.step);
      snprintf(frequency,sizeof(frequency),"%lu.%06lu MHz",(unsigned long)(radioHUD.frequency/1000000),(unsigned long)(radioHUD.frequency%1000000));
      u8g2.drawStr(2,37,frequency);
      if (radioHUD.volume<0) snprintf(detail,sizeof(detail),"%s S:%s V:--",radioHUD.mode,stepText);
      else snprintf(detail,sizeof(detail),"%s S:%s V:%d%s",radioHUD.mode,stepText,radioHUD.volume,radioHUD.muted?"M":"%");
      u8g2.drawStr(2,45,detail);
    } else {
      u8g2.drawStr(2,38,"Waiting for console");
      u8g2.drawStr(2,45,"Hold8:profiles 7:setup");
    }
  } else if (profile.displayScene == 0) {
    clipped(3, 28, profile.name, 112);
    clipped(3, 43, profile.displayText, 124);
  }
  else if (!petTalking && (profile.displayScene == 1 || profile.displayScene == 2))
    drawPixelPet([](int x, int y) { u8g2.drawPixel(x, y); }, screenFrameMs, profile.displayScene == 2, petTalking);
  if (!radioHUD.supported(profile.name) && profile.displayScene != 0) {
    u8g2.setFont(u8g2_font_wqy12_t_gb2312); clipped(2,28,profile.name,114);
  }
  for (int i = 0; i < 6; ++i) {
    int x = i % 3 * 43, y = i < 3 ? 0 : 48;
    u8g2.setFont(u8g2_font_5x7_tf);
    char number[2] = {(char)('1' + i), 0}; u8g2.drawStr(x + 1, y + 12, number);
    u8g2.setFont(u8g2_font_wqy12_t_gb2312);
    clipped(x + 8, y + 12, profile.labels[i], 33);
  }
  if (!radioHUD.supported(profile.name) && petTalking && (profile.displayScene == 1 || profile.displayScene == 2)) {
    // Two lines fit the centre strip; longer custom quotes expand vertically.
    const int height = petSpeechPageCount * 12 + 6;
    const int top = (64 - height) / 2;
    u8g2.setDrawColor(0); u8g2.drawBox(0, top, 128, height);
    u8g2.setDrawColor(1); u8g2.drawRFrame(0, top, 128, height, 2);
    u8g2.setFont(u8g2_font_wqy12_t_gb2312);
    for (unsigned row = 0; row < petSpeechPageCount; ++row)
      clipped(4, top + 13 + row * 12, petSpeechPages[row].c_str(), 120);
  }

}
