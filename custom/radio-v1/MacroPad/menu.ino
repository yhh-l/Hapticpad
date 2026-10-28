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
void replaceSetting(String &block, const char *name, const String &value) {
  String start=String("<")+name+">", end=String("</")+name+">";
  int a=block.indexOf(start), b=a<0?-1:block.indexOf(end,a+start.length());
  if (a>=0 && b>=a) block=block.substring(0,a+start.length())+value+block.substring(b);
  else {
    int close=block.indexOf("</Profile>");
    if(close>=0)block=block.substring(0,close)+start+value+end+block.substring(close);
  }
}
bool persistSettingsXML(const String &candidate) {
  if (!candidate.length() || candidate.length()>65536) return false;
  const char *temp="/config.menu.tmp", *backup="/config.menu.bak";
  FsFile file=SD.open(temp,O_WRONLY|O_CREAT|O_TRUNC);
  if (!file) return false;
  bool written=file.write((const uint8_t*)candidate.c_str(),candidate.length())==candidate.length();
  written=file.sync() && written; file.close();
  if (!written) {SD.remove(temp);return false;}
  file=SD.open(temp,O_RDONLY);
  if (!file || file.size()!=candidate.length()) {if(file)file.close();SD.remove(temp);return false;}
  uint8_t buffer[128]; unsigned position=0;
  while(position<candidate.length()) {
    unsigned count=min(128u,unsigned(candidate.length()-position));
    if(file.read(buffer,count)!=(int)count || memcmp(buffer,candidate.c_str()+position,count)) {
      file.close();SD.remove(temp);return false;
    }
    position+=count;
  }
  file.close();
  // Keep the old complete file until the new complete file is verified.
  if(SD.exists(backup) && !SD.remove(backup))return false;
  if(!SD.rename("/config.xml",backup))return false;
  if(!SD.rename(temp,"/config.xml")){SD.rename(backup,"/config.xml");return false;}
  return true;
}
bool saveMenuSettings() {
  if (settingsMenu.editing && settingsMenu.item==0) {confirmMenuField();if(settingsMenu.error)return false;}
  const MenuValues &value=settingsMenu.values;
  String block=profileBlock(value.profile);
  if(!block.length())return false;
  replaceSetting(block,"HapticStrength",String(value.strength));
  replaceSetting(block,"HapticDamping",String(value.damping));
  replaceSetting(block,"LED_Brightness",String(value.brightness));
  const char *ledNames[]={"Halo","Breath","Bands","Rainbow","Solid","Off","Knob","KnobTrail"};
  replaceSetting(block,"LED_Mode",ledNames[value.led]);
  // Passive damping/assist profiles require Clicky, as in the web editor.
  String wheel=textTag(block,"HapticMode","Follow")=="Follow" ?
    value.wheel==1?"Twist":value.wheel==2?"Momentum":value.wheel==3?"Flywheel":"Clicky" : "Clicky";
  replaceSetting(block,"WheelMode",wheel);
  String before=profileBlock(value.profile);
  int position=configXML.indexOf(before);
  if(position<0)return false;
  String candidate=configXML.substring(0,position)+block+configXML.substring(position+before.length());
  if(!persistSettingsXML(candidate))return false;
  configXML=candidate; return commitProfile(value.profile);
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
    u8g2.drawUTF8(0,12,settingsMenu.error?"切换失败 8重试":"选配置 7返回 8确认");
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
    u8g2.drawUTF8(0,48,"7返回 8确认");
  } else {
    int first=constrain(settingsMenu.item-1,0,3);
    for(int row=0;row<3;++row){
      int item=first+row,y=27+row*12;
      if(item==settingsMenu.item){u8g2.drawBox(0,y-11,128,12);u8g2.setDrawColor(0);}
      clipped(2,y,names[item],124);u8g2.setDrawColor(1);
    }
  }
  u8g2.drawUTF8(0,64,"长7放弃 长8保存");
}
