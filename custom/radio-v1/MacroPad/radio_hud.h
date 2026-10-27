#pragma once
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>

struct RadioHUD {
  uint32_t frequency=0, step=100, receivedAt=0;
  int volume=-1;
  bool valid=false, muted=false;
  char mode[9]={};
  static const char *profileFor(const char *target) {
    if (!strcmp(target,"onair")) return "OnAir收听";
    if (!strcmp(target,"neuro")) return "NeuroSDR收听";
    if (!strcmp(target,"sharp")) return "SDR井号收听";
    if (!strcmp(target,"nexus")) return "Nexus收听";
    if (!strcmp(target,"wfview")) return "wfview收听";
    return "";
  }
  static bool supported(const char *profile) {
    const char *targets[]={"onair","neuro","sharp","nexus","wfview"};
    for (const char *t:targets) if (!strcmp(profileFor(t),profile)) return true;
    return false;
  }
  void clear() { valid=false; }
  bool fresh(uint32_t now) const { return valid && uint32_t(now-receivedAt)<5000; }
  bool update(const char *line, const char *profile, uint32_t now) {
    if (strlen(line)>160) return false;
    char text[161]; strcpy(text,line);
    char *parts[6]; parts[0]=text;
    for (int i=1;i<6;++i) {
      char *sep=strchr(parts[i-1],'|'); if(!sep)return false;
      *sep=0; parts[i]=sep+1;
    }
    if(strchr(parts[5],'|') || !*profileFor(parts[0]) || strcmp(profileFor(parts[0]),profile))return false;
    auto number=[](const char *s,long low,long high,long &out) {
      if(!*s || (*s!='-' && (*s<'0'||*s>'9')))return false;
      char *end=nullptr;out=strtol(s,&end,10);
      return end && !*end && out>=low && out<=high;
    };
    long f,s,v,m;
    if(!number(parts[1],1000,2000000000L,f) || !number(parts[3],1,100000,s) ||
       !number(parts[4],-1,100,v) || !number(parts[5],0,1,m))return false;
    if(!*parts[2] || strlen(parts[2])>8)return false;
    for(const char *p=parts[2];*p;++p)if(!((*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||*p=='-'))return false;
    frequency=f;step=s;volume=v;muted=m;strcpy(mode,parts[2]);receivedAt=now;valid=true;return true;
  }
};
