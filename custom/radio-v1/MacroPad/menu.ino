MenuValues menuValues(int index, const Profile &source) {
  MenuValues values;
  values.profile=index; values.strength=source.feedback.strength;
  values.damping=source.feedback.damping; values.led=source.ledMode;
  values.brightness=source.brightness; values.wheel=source.mode;
  values.follow=source.feedback.mode==FOLLOW;
  return values;
}
void enterProfileSelector() {
  releaseKeys(); menuKind=1; menuSelection=activeProfile; settingsMenu.error=false;
  chordRecognizer.cancel(buttonsMask); wheelEvents.store(0);
  menuOpen.store(true); generation.fetch_add(1);
}
void enterSettings() {
  menuKind=2; releaseKeys(); settingsMenu.begin(menuValues(activeProfile,profile));
  chordRecognizer.cancel(buttonsMask); wheelEvents.store(0);
  menuOpen.store(true); generation.fetch_add(1);
}
void confirmMenuField() {
  if (settingsMenu.item==0) {
    std::unique_ptr<Profile> selected(new (std::nothrow) Profile);
    if (!selected || !readProfile(settingsMenu.values.profile,*selected)) {settingsMenu.error=true;return;}
    settingsMenu.values=menuValues(settingsMenu.values.profile,*selected);
  }
  settingsMenu.confirm();
}
bool replaceSetting(String &block, const char *name, const String &value) {
  String start=String("<")+name+">", end=String("</")+name+">";
  if(start.length()!=strlen(name)+2 || end.length()!=strlen(name)+3)return false;
  int a=block.indexOf(start), b=a<0?-1:block.indexOf(end,a+start.length());
  unsigned position=0,remove=0;String insertion;
  if(a>=0 && b>=a){position=a+start.length();remove=b-position;insertion=value;if(insertion.length()!=value.length())return false;}
  else {
    int close=block.indexOf("</Profile>");if(close<0)return false;
    position=close;insertion=start+value+end;
    if(insertion.length()!=start.length()+value.length()+end.length())return false;
  }
  unsigned expected=block.length()-remove+insertion.length();
  if(expected>SD_MAX_PROFILE_BYTES){return false;}
  // Update in place: total free heap does not guarantee another contiguous
  // 64KiB block. Reserve before mutation so failure preserves the input.
  unsigned oldLength=block.length();
  if(!block.reserve(expected)){return false;}
  if(expected>oldLength && !block.concat(insertion.c_str(),expected-oldLength))return false;
  char *data=block.begin();
  memmove(data+position+insertion.length(),data+position+remove,oldLength-position-remove);
  memcpy(data+position,insertion.c_str(),insertion.length());
  if(expected<oldLength)block.remove(expected);
  return block.length()==expected;
}
bool copyConfigBytes(FsFile &source,FsFile &target,uint32_t begin,uint32_t count) {
  if(!source.seekSet(begin))return false;
  uint8_t buffer[2048];
  observeStorageStack();
  while(count){
    unsigned n=min(count,uint32_t(sizeof(buffer)));
    if(source.read(buffer,n)!=(int)n || target.write(buffer,n)!=n)return false;
    count-=n;
  }
  return true;
}
bool compareConfigBytes(FsFile &a,FsFile &b,uint32_t first,uint32_t second,uint32_t count) {
  if(!a.seekSet(first)||!b.seekSet(second))return false;
  uint8_t left[1024],right[1024];
  observeStorageStack();
  while(count){unsigned n=min(count,uint32_t(sizeof(left)));if(a.read(left,n)!=(int)n||b.read(right,n)!=(int)n||memcmp(left,right,n))return false;count-=n;}
  return true;
}
bool persistProfileXML(unsigned index,const String &replacement) {
  if(index>=configIndex.count || !replacement.length() || replacement.length()>SD_MAX_PROFILE_BYTES)return false;
  ConfigSpan span=configIndex.profiles[index];
  uint32_t expected=configIndex.bytes-span.size()+replacement.length();
  if(expected>SD_MAX_CONFIG_BYTES)return false;
  const char *temp="/config.menu.tmp",*backup="/config.menu.bak";
  FsFile original=SD.open("/config.xml",O_RDONLY);
  FsFile file=SD.open(temp,O_WRONLY|O_CREAT|O_TRUNC);
  if(!original || !file || original.size()!=configIndex.bytes)return false;
  bool written=copyConfigBytes(original,file,0,span.begin) &&
    file.write((const uint8_t*)replacement.c_str(),replacement.length())==replacement.length() &&
    copyConfigBytes(original,file,span.end,configIndex.bytes-span.end);
  written=file.sync() && written;file.close();
  if(!written){original.close();SD.remove(temp);return false;}
#if defined(ARDUINO_ARCH_RP2040)
  message("正在核对配置", "请稍候 勿断电");
#endif
  file=SD.open(temp,O_RDONLY);
  bool same=file && file.size()==expected && compareConfigBytes(original,file,0,0,span.begin);
  uint8_t buffer[128];unsigned position=0;
  while(same && position<replacement.length()){
    unsigned n=min(128u,unsigned(replacement.length()-position));
    same=file.read(buffer,n)==(int)n && !memcmp(buffer,replacement.c_str()+position,n);position+=n;
  }
  same=same && compareConfigBytes(original,file,span.end,span.begin+replacement.length(),configIndex.bytes-span.end);
  original.close();
  std::unique_ptr<ConfigIndex> candidate(new(std::nothrow) ConfigIndex);
  same=same && candidate && indexConfigFile(file,*candidate) && candidate->count==configIndex.count;
  file.close();
  if(!same){SD.remove(temp);return false;}
  if(SD.exists(backup) && !SD.remove(backup)){return false;}
  if(!SD.rename("/config.xml",backup)){return false;}
  if(!SD.rename(temp,"/config.xml")){SD.rename(backup,"/config.xml");return false;}
  configIndex=*candidate;
  return true;
}
bool saveMenuSettings() {
  if (settingsMenu.editing && settingsMenu.item==0) {confirmMenuField();if(settingsMenu.error)return false;}
  const MenuValues &value=settingsMenu.values;
#if defined(ARDUINO_ARCH_RP2040)
  message("正在保存配置", "请稍候 勿断电");
#endif
  { // Release the editing XML before commitProfile allocates the parser input.
  String block=profileBlock(value.profile);
  if(!block.length())return false;
  if(!replaceSetting(block,"HapticStrength",String(value.strength)))return false;
  if(!replaceSetting(block,"HapticDamping",String(value.damping)))return false;
  if(!replaceSetting(block,"LED_Brightness",String(value.brightness)))return false;
  const char *ledNames[]={"Halo","Breath","Bands","Rainbow","Solid","Off","Knob","KnobTrail"};
  if(!replaceSetting(block,"LED_Mode",ledNames[value.led]))return false;
  // Passive damping/assist profiles require Clicky, as in the web editor.
  String wheel=textTag(block,"HapticMode","Follow")=="Follow" ?
    value.wheel==1?"Twist":value.wheel==2?"Momentum":value.wheel==3?"Flywheel":"Clicky" : "Clicky";
  if(!replaceSetting(block,"WheelMode",wheel))return false;
  if(!persistProfileXML(value.profile,block))return false;
  }
  return commitProfile(value.profile);
}
String menuValueText() {
  const MenuValues &value=settingsMenu.values;
  switch(settingsMenu.item) {
    case 0:return String(profileNames[value.profile]);
    case 1:return String(value.strength)+" / 100";
    case 2:return String(value.damping)+" / 100";
    case 3:{const char *names[]={"光环","呼吸","灯带","彩虹","常亮","关闭","固定跟随","彩色拖尾"};return names[value.led];}
    case 4:return String(value.brightness)+" / 255";
    default:return value.wheel==1?"回中模式":value.wheel==2?"惯性模式":value.wheel==3?"无极续转":"刻度模式";
  }
}
void drawSettings() {
  if(menuKind==1){
    u8g2.setFont(u8g2_font_wqy12_t_gb2312);
    u8g2.drawUTF8(0,12,settingsMenu.error?"切换失败 7重试":"选配置 8返回 7确认");
    for(int row=-1;row<=1;++row){
      int index=menuSelection+row,y=39+row*14;
      if(index<0 || index>=profileCount)continue;
      if(!row){u8g2.drawBox(0,y-12,128,14);u8g2.setDrawColor(0);}
      clipped(2,y,profileNames[index],124);u8g2.setDrawColor(1);
    }
    return;
  }
  const char *names[]={"1 配置选择","2 力反馈力度","3 旋转阻尼","4 灯光效果","5 灯光亮度","6 旋钮模式"};
  u8g2.setFont(u8g2_font_wqy12_t_gb2312);
  if(settingsMenu.error){u8g2.drawUTF8(0,12,"保存失败 请重试");}
  else u8g2.drawUTF8(0,12,settingsMenu.editing?names[settingsMenu.item]:"设置 旋转或按数字");
  if(settingsMenu.editing){
    clipped(2,34,menuValueText().c_str(),124);
    u8g2.drawUTF8(0,48,"8返回 7确认");
  } else {
    int first=constrain(settingsMenu.item-1,0,3);
    for(int row=0;row<3;++row){
      int item=first+row,y=27+row*12;
      if(item==settingsMenu.item){u8g2.drawBox(0,y-11,128,12);u8g2.setDrawColor(0);}
      clipped(2,y,names[item],124);u8g2.setDrawColor(1);
    }
  }
  u8g2.drawUTF8(0,64,"长8放弃 长7保存");
}
