FeiyuStore<SdFat> feiyuMemory(SD);
FeiyuSaveGesture feiyuSaveGesture;
bool feiyuSaveEnabled=false,feiyuHostSeen=false,feiyuSavePending=false;
uint16_t feiyuSaveHoldMs=1000;
uint32_t feiyuSaveRequest=0,feiyuHostAt=0,feiyuTransferAt=0,feiyuNoticeAt=0;
const char *feiyuState="idle";

void setupFeiyu() {
  feiyuSaveEnabled=textTag(globalSettings,"FeiyuSaveEnabled","0")=="1";
  // This revision reserves exactly 1+2+3, keeping all small-key navigation.
  if(textTag(globalSettings,"FeiyuSaveMask","7")!="7")feiyuSaveEnabled=false;
  long hold=textTag(globalSettings,"FeiyuSaveHoldMs","1000").toInt();
  feiyuSaveHoldMs=constrain(hold,700,3000);
  if(sdDetected)feiyuMemory.begin();
}
void requestFeiyuSave() {
  if(!++feiyuSaveRequest)++feiyuSaveRequest;
  feiyuSavePending=true;feiyuNoticeAt=millis();
  feiyuState=feiyuHostSeen && uint32_t(millis()-feiyuHostAt)<5000?"waiting":"offline";
}
bool consumeFeiyuGesture(uint8_t buttons,uint32_t now) {
  bool wasActive=feiyuSaveGesture.active;
  int result=feiyuSaveGesture.update(buttons,now,feiyuSaveEnabled,feiyuSaveHoldMs);
  if(!result)return false;
  if(!wasActive)releaseKeys();
  if(result==2)requestFeiyuSave();
  return true;
}
void tickFeiyu() {
  if(feiyuMemory.writing && uint32_t(millis()-feiyuTransferAt)>30000){
    feiyuMemory.abort();feiyuState="failed";feiyuNoticeAt=millis();
  }
  if(feiyuSavePending && (!feiyuHostSeen || uint32_t(millis()-feiyuHostAt)>5000))feiyuState="offline";
}
bool drawFeiyuNotice() {
  if(!feiyuNoticeAt || uint32_t(millis()-feiyuNoticeAt)>5000)return false;
  const char *text=!strcmp(feiyuState,"saved")?"记忆已存SD卡":
    !strcmp(feiyuState,"failed")?"记忆保存失败":
    !strcmp(feiyuState,"offline")?"助手未连接":"等待助手保存";
  drawFeiyuCharacter(!strcmp(feiyuState,"saved")?FEIYU_HAPPY:FEIYU_THINK);
  (void)text;
  return true;
}
bool feiyuNumber(const char *&p,uint32_t &value,unsigned base=10) {
  if(!*p)return false;value=0;unsigned digits=0;
  while(*p && *p!=' '){
    int d=*p>='0'&&*p<='9'?*p-'0':(*p>='a'&&*p<='f'?*p-'a'+10:(*p>='A'&&*p<='F'?*p-'A'+10:-1));
    if(d<0 || unsigned(d)>=base || value>(0xffffffffu-unsigned(d))/base)return false;
    value=value*base+unsigned(d);++p;++digits;
  }
  return digits>0;
}
void feiyuReply(bool ok,const char *operation) {
  Serial.print(ok?"HP MEM OK ":"HP MEM ERROR ");Serial.println(ok?operation:feiyuMemory.error());
}
bool handleFeiyuCommand(const char *command) {
  if(strncmp(command,"HP MEM ",7))return false;
  if(diskMode.load() || !sdDetected){Serial.println("HP MEM ERROR MODE");return true;}
  const char *p=command+7;
  if(!strcmp(p,"INFO")){
    feiyuHostSeen=true;feiyuHostAt=millis();
    Serial.print("HP MEM INFO {\"protocol\":1,\"generation\":");Serial.print(feiyuMemory.generation);
    Serial.print(",\"size\":");Serial.print(feiyuMemory.size);
    Serial.print(",\"crc32\":");Serial.print(feiyuMemory.crc);
    Serial.print(",\"max_bytes\":");Serial.print(feiyuMemory.MAX_BYTES);
    Serial.print(",\"save_request\":");Serial.print(feiyuSaveRequest);
    Serial.print(",\"save_pending\":");Serial.print(feiyuSavePending?"true":"false");
    Serial.print(",\"save_enabled\":");Serial.print(feiyuSaveEnabled?"true":"false");
    Serial.print(",\"save_mask\":7,\"save_hold_ms\":");Serial.print(feiyuSaveHoldMs);
    Serial.print(",\"state\":\"");Serial.print(feiyuState);Serial.println("\"}");return true;
  }
  if(!strcmp(p,"SAVE")){requestFeiyuSave();Serial.println("HP MEM OK SAVE");return true;}
  if(!strcmp(p,"ABORT")){feiyuMemory.abort();Serial.println("HP MEM OK ABORT");return true;}
  if(!strcmp(p,"COMMIT")){feiyuReply(feiyuMemory.commit(),"COMMIT");return true;}
  if(!strncmp(p,"READ ",5)){
    p+=5;uint32_t offset=0;
    if(!feiyuNumber(p,offset) || *p){Serial.println("HP MEM ERROR ARGUMENT");return true;}
    uint8_t bytes[128];int n=feiyuMemory.read(offset,bytes,sizeof(bytes));
    if(n<0){Serial.println("HP MEM ERROR READ");return true;}
    Serial.print("HP MEM DATA ");Serial.print(offset);Serial.print(' ');
    const char *hex="0123456789abcdef";
    for(int i=0;i<n;++i){Serial.print(hex[bytes[i]>>4]);Serial.print(hex[bytes[i]&15]);}
    Serial.println();return true;
  }
  if(!strncmp(p,"BEGIN ",6)){
    p+=6;uint32_t size=0,crc=0,expected=0;
    if(!feiyuNumber(p,size) || *p++!=' ' || !feiyuNumber(p,crc,16) || *p++!=' ' || !feiyuNumber(p,expected) || *p){Serial.println("HP MEM ERROR ARGUMENT");return true;}
    feiyuTransferAt=millis();feiyuReply(feiyuMemory.start(size,crc,expected),"BEGIN");return true;
  }
  if(!strncmp(p,"WRITE ",6)){
    p+=6;uint32_t offset=0;
    if(!feiyuNumber(p,offset) || *p++!=' '){Serial.println("HP MEM ERROR ARGUMENT");return true;}
    size_t length=strlen(p);uint8_t bytes[128];
    if(!length || length>256 || length%2){Serial.println("HP MEM ERROR ARGUMENT");return true;}
    for(unsigned i=0;i<length/2;++i){
      unsigned value=0;for(unsigned j=0;j<2;++j){char c=p[i*2+j];int d=c>='0'&&c<='9'?c-'0':(c>='a'&&c<='f'?c-'a'+10:(c>='A'&&c<='F'?c-'A'+10:-1));if(d<0){Serial.println("HP MEM ERROR ARGUMENT");return true;}value=value*16+unsigned(d);}bytes[i]=value;
    }
    feiyuTransferAt=millis();feiyuReply(feiyuMemory.write(offset,bytes,length/2),"WRITE");return true;
  }
  if(!strncmp(p,"ACK ",4)){
    p+=4;uint32_t request=0,generation=0;
    if(feiyuNumber(p,request) && *p++==' ' && feiyuNumber(p,generation) && !*p && feiyuSavePending && request==feiyuSaveRequest && generation>0 && generation==feiyuMemory.generation){
      feiyuSavePending=false;feiyuState="saved";feiyuNoticeAt=millis();Serial.println("HP MEM OK ACK");
    }else Serial.println("HP MEM ERROR ACK");return true;
  }
  if(!strncmp(p,"FAIL ",5)){
    p+=5;uint32_t request=0;
    if(feiyuNumber(p,request) && !*p && feiyuSavePending && request==feiyuSaveRequest){feiyuSavePending=false;feiyuState="failed";feiyuNoticeAt=millis();Serial.println("HP MEM OK FAIL");}
    else Serial.println("HP MEM ERROR ACK");return true;
  }
  Serial.println("HP MEM ERROR COMMAND");return true;
}
