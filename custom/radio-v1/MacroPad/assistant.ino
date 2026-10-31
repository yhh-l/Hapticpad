FeiyuAssistant feiyuAssistant;
bool feiyuNumber(const char *&p,uint32_t &value,unsigned base);

bool consumeAssistantGesture(uint8_t mask,uint32_t now) {
  bool consumed=feiyuAssistant.buttons(mask,now);
  if(consumed)releaseKeys();
  return consumed;
}
void turnAssistant(int steps){feiyuAssistant.turn(steps);}
bool handleAssistantCommand(const char *command){
  if(strncmp(command,"HP AI ",6))return false;
  const char *p=command+6;
  if(!strcmp(p,"INFO")){
    Serial.print("HP AI INFO {\"protocol\":1,\"mode\":");Serial.print(feiyuAssistant.mode?"true":"false");
    Serial.print(",\"event_id\":");Serial.print(feiyuAssistant.count?feiyuAssistant.queue[feiyuAssistant.head].id:0);
    Serial.print(",\"action\":\"");Serial.print(feiyuAssistant.count?feiyuAssistant.queue[feiyuAssistant.head].action:"");
    Serial.print("\",\"phase\":\"");Serial.print(feiyuAssistant.fresh(millis())?feiyuAssistant.phase:"offline");
    Serial.print("\",\"page\":");Serial.print(feiyuAssistant.pageIndex);
    Serial.print(",\"pages\":");Serial.print(feiyuAssistant.pageCount);Serial.println("}");return true;
  }
  if(!strncmp(p,"STATE ",6)){
    Serial.println(feiyuAssistant.status(p+6,millis())?"HP AI OK STATE":"HP AI ERROR STATE");return true;
  }
  if(!strncmp(p,"ACK ",4)){
    p+=4;uint32_t id=0;
    if(feiyuNumber(p,id,10) && !*p && feiyuAssistant.count && feiyuAssistant.queue[feiyuAssistant.head].id==id){feiyuAssistant.ack(id);Serial.println("HP AI OK ACK");}
    else Serial.println("HP AI ERROR ACK");return true;
  }
  if(!strncmp(p,"PAGE ",5)){
    p+=5;uint32_t index=0,total=0;
    if(!feiyuNumber(p,index,10) || *p++!=' ' || !feiyuNumber(p,total,10) || *p++!=' ' || !total || total>512 || index>=total){Serial.println("HP AI ERROR PAGE");return true;}
    size_t size=strlen(p);char bytes[128]={};
    if(!size || size%2 || size>250){Serial.println("HP AI ERROR PAGE");return true;}
    for(unsigned i=0;i<size/2;++i){
      unsigned value=0;
      for(unsigned j=0;j<2;++j){char c=p[2*i+j];int d=c>='0'&&c<='9'?c-'0':(c>='a'&&c<='f'?c-'a'+10:-1);if(d<0){Serial.println("HP AI ERROR PAGE");return true;}value=value*16+d;}
      if(!value || (value<32 && value!='\n')){Serial.println("HP AI ERROR PAGE");return true;}bytes[i]=char(value);
    }
    memcpy(feiyuAssistant.page,bytes,sizeof(bytes));feiyuAssistant.pageIndex=index;feiyuAssistant.pageCount=total;
    Serial.println("HP AI OK PAGE");return true;
  }
  if(!strcmp(p,"MODE 1") || !strcmp(p,"MODE 0")){
    feiyuAssistant.mode=p[5]=='1';feiyuAssistant.armed=false;Serial.println("HP AI OK MODE");return true;
  }
  Serial.println("HP AI ERROR COMMAND");return true;
}
const char *assistantLabel(){
  if(!feiyuAssistant.fresh(millis()))return "助手未连接";
  const char *p=feiyuAssistant.phase;
  return !strcmp(p,"thinking")?"正在思考":!strcmp(p,"reply")?"正在回复":!strcmp(p,"tool")?"正在执行工具":
    !strcmp(p,"done")?"任务已完成":!strcmp(p,"saving")?"正在整理记忆":!strcmp(p,"error")?"任务出错":!strcmp(p,"cancelled")?"任务已停止":"助手已连接";
}
bool drawAssistantTerminal(){
  if(!feiyuAssistant.mode)return false;
  u8g2.setFont(u8g2_font_wqy12_t_gb2312);
  u8g2.drawUTF8(0,12,assistantLabel());
  char marker[18];snprintf(marker,sizeof(marker),"%u/%u",feiyuAssistant.pageIndex+1,feiyuAssistant.pageCount);
  u8g2.drawUTF8(128-u8g2.getUTF8Width(marker),12,marker);u8g2.drawHLine(0,16,128);
  const char *content=feiyuAssistant.fresh(millis()) && feiyuAssistant.page[0]?feiyuAssistant.page:"1打开 2续接 3保存\n4上页 5下页 6退出\n旋钮翻页 123保存";
  for(unsigned line=0;line<3 && *content;++line){
    char row[128]={};unsigned n=0;while(*content && *content!='\n' && n<sizeof(row)-1)row[n++]=*content++;
    if(*content=='\n')++content;
    clipped(0,32+line*14,row,128);
  }
  return true;
}
