#pragma once
#include <cstdint>
#include <cstring>

// File offsets only: no profile XML is retained by the index.
constexpr unsigned SD_MAX_PROFILES = 256;
constexpr uint32_t SD_MAX_CONFIG_BYTES = 8u * 1024u * 1024u;
constexpr uint32_t SD_MAX_PROFILE_BYTES = 65536;
constexpr uint32_t SD_MAX_SETTINGS_BYTES = 4096;
struct ConfigSpan { uint32_t begin=0, end=0; uint32_t size() const { return end-begin; } };
struct ConfigIndex {
  ConfigSpan profiles[SD_MAX_PROFILES], settings;
  unsigned count=0;
  uint32_t bytes=0;
};

// Reader supplies read() -> byte/-1 and position() -> consumed byte offset.
// Match complete XML elements, including comments and quoted '>' characters.
// Unsupported declarations, oversized tags, nesting, or counts fail explicitly.
template<class Reader> bool scanConfig(Reader &reader, ConfigIndex &out) {
  out=ConfigIndex{};
  char stack[16][48]={}, token[512]; unsigned depth=0;
  bool rootSeen=false, rootClosed=false, profilesSeen=false, settingsSeen=false;
  auto space=[](char c){return c==' ' || c=='\t' || c=='\r' || c=='\n';};
  auto get=[&](){int c=reader.read();return reader.position()>SD_MAX_CONFIG_BYTES?-2:c;};
  int c;
  while ((c=get())>=0) {
    if(reader.position()==1 && c==0xef){if(get()!=0xbb || get()!=0xbf)return false;continue;}
    if(c!='<') { if((!depth || depth==1 || (depth==2 && !strcmp(stack[1],"Profiles"))) && !space(c))return false; continue; }
    uint32_t start=reader.position()-1;
    unsigned length=0; char quote=0;
    while ((c=get())>=0) {
      if(length+1>=sizeof(token))return false;
      token[length++]=char(c);token[length]=0;
      if(length==3 && !strcmp(token,"!--")) {
        int a=0,b=0;
        while((c=get())>=0){if(a=='-'&&b=='-'&&c=='>')break;a=b;b=c;}
        if(c<0)return false;
        length=0;break;
      }
      if(quote){if(c==quote)quote=0;}
      else if(c=='"'||c=='\'')quote=char(c);
      else if(c=='>')break;
    }
    if(c<0)return false;
    if(!length)continue; // Comment.
    if(token[0]=='?') {
      if(rootSeen || length<2 || token[length-2]!='?')return false;
      continue;
    }
    if(token[0]=='!')return false; // No DTD/entities/CDATA.
    bool closing=token[0]=='/';unsigned p=closing?1:0,begin=p;
    while(p<length && !space(token[p]) && token[p]!='/' && token[p]!='>')++p;
    if(p==begin || p-begin>=sizeof(stack[0]))return false;
    char name[48]={};memcpy(name,token+begin,p-begin);
    bool empty=length>=2 && token[length-2]=='/';
    if(closing) {
      for(unsigned j=p;j+1<length;++j)if(!space(token[j]))return false;
      if(!depth || strcmp(stack[depth-1],name))return false;
      if(depth==3 && !strcmp(stack[1],"Profiles") && !strcmp(name,"Profile")) {
        auto &span=out.profiles[out.count-1];span.end=reader.position();
        if(span.size()>SD_MAX_PROFILE_BYTES)return false;
      }
      if(depth==2 && !strcmp(name,"Settings")) {
        out.settings.end=reader.position();if(out.settings.size()>SD_MAX_SETTINGS_BYTES)return false;
      }
      --depth;if(!depth)rootClosed=true;
      continue;
    }
    if(depth>=16 || rootClosed)return false;
    if(!depth){if(rootSeen || strcmp(name,"Configuration"))return false;rootSeen=true;}
    else if(depth==1) {
      if(!strcmp(name,"Profiles")){if(profilesSeen||empty)return false;profilesSeen=true;}
      else if(!strcmp(name,"Settings")){if(settingsSeen)return false;settingsSeen=true;out.settings.begin=start;if(empty)out.settings.end=reader.position();}
      else return false;
    } else if(depth==2 && !strcmp(stack[1],"Profiles")) {
      if(strcmp(name,"Profile") || empty || out.count>=SD_MAX_PROFILES)return false;
      out.profiles[out.count++].begin=start;
    }
    strcpy(stack[depth++],name);
    if(empty)--depth;
  }
  out.bytes=reader.position();
  return c==-1 && rootSeen && rootClosed && !depth && profilesSeen && out.count;
}
