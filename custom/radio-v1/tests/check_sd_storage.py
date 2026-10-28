"""Compile production indexed parser and transactional streaming saver with fake FAT.
Usage: python3 tests/check_sd_storage.py /path/to/ArduinoCore-API/api
"""
from pathlib import Path
import subprocess,sys,tempfile,re
root=Path(__file__).resolve().parents[1];api=Path(sys.argv[1]).resolve()
source=(root/'MacroPad/config.ino').read_text().split('void setLEDProfile()')[0]
source=source.replace('const String &fallback = ""','const String &fallback')
menu=(root/'MacroPad/menu.ino').read_text()
source+=menu[menu.index('bool replaceSetting('):menu.index('bool saveMenuSettings(')]
header=r'''
#include <string>
#include <map>
#include <memory>
#include <iostream>
#include <cstring>
#include <cassert>
#include <cmath>
#include "String.h"
#include "Common.h"
#include "device.h"
using namespace arduino;
String textTag(const String &,const char *,const String &fallback="");
template<class T>T min(T a,T b){return a<b?a:b;}
constexpr int O_RDONLY=0,O_WRONLY=1,O_CREAT=2,O_TRUNC=4;
std::map<std::string,std::string> files;
int writeLimit=-1,failRename=0,renames=0;bool syncOK=true,corrupt=false;
size_t readBytes=0;
std::map<void*,size_t> &allocations(){static auto *m=new std::map<void*,size_t>;return *m;}
size_t xmlBytes=0,xmlPeak=0,xmlLimit=SIZE_MAX;
void *storageRealloc(void *old,size_t size){
 size_t previous=old?allocations().at(old):0;
 if(xmlBytes-previous+size>xmlLimit)return nullptr;
 void *next=realloc(old,size);if(!next)return nullptr;
 if(old)allocations().erase(old);allocations()[next]=size;xmlBytes=xmlBytes-previous+size;xmlPeak=std::max(xmlPeak,xmlBytes);return next;
}
void storageFree(void *p){if(p){xmlBytes-=allocations().at(p);allocations().erase(p);}free(p);}

struct FsFile{
 std::string path;size_t pos=0;bool valid=false;
 FsFile()=default;FsFile(std::string p):path(p),valid(true){}
 explicit operator bool()const{return valid;}
 size_t size()const{return valid?files[path].size():0;}
 bool seekSet(uint32_t p){if(!valid||p>size())return false;pos=p;return true;}
 int read(void*out,unsigned n){if(!valid)return -1;n=std::min(n,unsigned(size()-pos));memcpy(out,files[path].data()+pos,n);if(corrupt&&path=="/config.menu.tmp"&&n)((char*)out)[0]^=1;pos+=n;readBytes+=n;return n;}
 int read(){unsigned char c;return read(&c,1)==1?c:-1;}
 size_t write(const uint8_t*data,size_t n){if(!valid)return 0;if(writeLimit>=0)n=std::min(n,size_t(writeLimit));auto &s=files[path];if(pos+n>s.size())s.resize(pos+n);memcpy(s.data()+pos,data,n);pos+=n;return n;}
 bool sync(){return syncOK;}
 void close(){valid=false;}
};
struct FakeSD{
 FsFile open(const char*p,int mode=0){if(mode&O_CREAT)files[p];if(!files.count(p))return {};if(mode&O_TRUNC)files[p].clear();return FsFile(p);}
 bool exists(const char*p){return files.count(p);}
 bool remove(const char*p){return files.erase(p);}
 bool rename(const char*a,const char*b){++renames;if(renames==failRename||!files.count(a)||files.count(b))return false;files[b]=std::move(files[a]);files.erase(a);return true;}
}SD;
'''
main=r'''
std::string button="<MacroButton><Action>0,91</Action><Action>0,67</Action><Action>0,0</Action><Label>复制</Label></MacroButton>";
std::string pxml(int n,int chords=0){std::string s="<Profile name=\"Mac"+std::to_string(n)+"\"><MacroButtons>";for(int i=0;i<6;i++)s+=button;s+="</MacroButtons><Chords>";for(int m=1;m<256&&chords;m++)if(m&(m-1)){s+="<Chord mask=\""+std::to_string(m)+"\"><Action>0,17</Action><Action>0,67</Action><Action>0,0</Action><Label>组合</Label>";if(m==56)s+="<NumberText>813816</NumberText><EnterAfterText>1</EnterAfterText>";s+="</Chord>";--chords;}return s+"</Chords><Unknown>保留</Unknown></Profile>";}
std::string xml(int count,int chords=0){std::string s="<?xml version=\"1.0\" encoding=\"UTF-8\"?><Configuration><Settings><MotorEnabled>1</MotorEnabled></Settings><Profiles>";for(int i=0;i<count;i++)s+=pxml(i,chords);return s+"</Profiles></Configuration>";}
bool indexText(const std::string&s){files["/config.xml"]=s;auto f=SD.open("/config.xml");return indexConfigFile(f,configIndex);}
void reset(const std::string&s){files.clear();writeLimit=-1;failRename=renames=0;syncOK=true;corrupt=false;assert(indexText(s));globalSettings=readConfigSpan(configIndex.settings);}
int main(){
 std::string small=xml(2);reset(small);Profile p;assert(readProfile(0,p)&&!strcmp(p.name,"Mac0"));assert(p.actions[0][0]==91&&p.actions[0][1]==67);assert(!readProfile(2,p));
 std::string large=xml(256,200);assert(large.size()>4*1024*1024);reset(large);assert(configIndex.count==256&&configIndex.bytes==large.size());
 readBytes=0;assert(readProfile(255,p)&&!strcmp(p.name,"Mac255")&&p.chordCount==200);assert(readBytes<65536);
 bool number=false;for(unsigned i=0;i<p.chordCount;i++)if(p.chords[i].mask==56){number=true;assert(!strcmp(p.chords[i].numberText,"813816")&&p.chords[i].enterAfterText);}assert(number);
 std::string validNumeric=xml(1,120);reset(validNumeric);assert(readProfile(0,p));
 for(const auto &bad: {std::string("813x16"),std::string("1234567890123456")}){auto invalid=validNumeric;invalid.replace(invalid.find("813816"),6,bad);reset(invalid);assert(!readProfile(0,p));}
 auto invalidEnter=validNumeric;invalidEnter.replace(invalidEnter.find("<EnterAfterText>1"),17,"<EnterAfterText>2");reset(invalidEnter);assert(!readProfile(0,p));
 reset(large);
 // Near-limit editing block must still stream and update offsets correctly.
 auto tight=xml(2,200);auto close=tight.find("</Profile>");auto profileStart=tight.find("<Profile name");
 auto current=close+10-profileStart;tight.insert(close,std::string(65000-current,'x'));reset(tight);
 String maximum=profileBlock(0);xmlLimit=80000;xmlPeak=xmlBytes;
 assert(replaceSetting(maximum,"HapticStrength","75"));assert(replaceSetting(maximum,"HapticDamping","80"));assert(replaceSetting(maximum,"LED_Brightness","120"));assert(xmlPeak<=80000);
 std::string preserved=maximum.c_str();String longValue(std::string(400,'9').c_str());xmlLimit=xmlBytes+128;assert(!replaceSetting(maximum,"HapticStrength",longValue));assert(preserved==maximum.c_str());xmlLimit=SIZE_MAX;
 assert(replaceSetting(maximum,"HapticStrength","9"));assert(replaceSetting(maximum,"HapticStrength","75"));
 assert(persistProfileXML(0,maximum));assert(readProfile(0,p)&&p.feedback.strength==75);
 reset(large);
 auto span=configIndex.profiles[130];std::string before=files["/config.xml"];String block=profileBlock(130);replaceSetting(block,"HapticStrength","75");
 assert(persistProfileXML(130,block));std::string expected=before.substr(0,span.begin)+block.c_str()+before.substr(span.end);assert(files["/config.xml"]==expected&&files["/config.menu.bak"]==before);
 assert(readProfile(130,p)&&p.feedback.strength==75);assert(readProfile(255,p)&&!strcmp(p.name,"Mac255"));
 for(int mode=0;mode<5;mode++){reset(small);String b=profileBlock(1);replaceSetting(b,"HapticStrength","90");if(mode==0)writeLimit=3;if(mode==1)syncOK=false;if(mode==2)corrupt=true;if(mode==3)failRename=1;if(mode==4)failRename=2;assert(!persistProfileXML(1,b));assert(files["/config.xml"]==small&&configIndex.count==2);}
 assert(!indexText(xml(257)));assert(!indexText("<Configuration><Profiles>"+pxml(0)));
 assert(!indexText("<!DOCTYPE Configuration><Configuration><Profiles>"+pxml(0)+"</Profiles></Configuration>"));
 assert(!indexText("<Configuration><Profiles>"+pxml(0)+"</Profiles></Wrong>"));
 assert(!indexText(xml(1)+xml(1)));assert(!indexText(xml(1)+"extra"));
 assert(!indexText("<Configuration><Settings>"+std::string(4097,'x')+"</Settings><Profiles>"+pxml(0)+"</Profiles></Configuration>"));
 assert(!indexText("<Configuration><Profiles><Profile name=\"x\">"+std::string(65536,'x')+"</Profile></Profiles></Configuration>"));
 assert(!indexText(std::string(8*1024*1024+1,' ')));reset(xml(1,247));assert(readProfile(0,p)&&p.chordCount==247);
 auto commented=xml(2);commented.insert(commented.find("<Profile name"),"<!-- <Profile name=\"fake\"> ignored </Profile> -->");assert(indexText(commented)&&configIndex.count==2);
 auto quoted=xml(1);quoted.replace(quoted.find("Mac0"),4,"Mac>0");assert(indexText(quoted)&&readProfile(0,p)&&!strcmp(p.name,"Mac>0"));assert(indexText(std::string("\xef\xbb\xbf")+xml(1)));
 std::cout<<"PASS: production SD index (>4MB, 256 profiles), bounded single-profile reads, 247 chords/numeric preservation, 80KB XML budget and allocation-failure preservation, exact streamed save, offset refresh, short writes/sync/corruption/rename rollback, malformed XML and all bounds\n";
}
'''
with tempfile.TemporaryDirectory(prefix='haptic-sd-storage-') as tmp:
 p=Path(tmp);(p/'check.cpp').write_text(header+source+main)
 stringSource=(api/'String.cpp').read_text()
 stringSource=re.sub(r'\brealloc\(', 'storageRealloc(',stringSource);stringSource=re.sub(r'\bfree\(', 'storageFree(',stringSource)
 stringSource=stringSource.replace('#include <float.h>', '#include <float.h>\nextern void *storageRealloc(void*,size_t);\nextern void storageFree(void*);')
 (p/'String.cpp').write_text(stringSource)
 subprocess.run(['clang++','-std=c++17','-iquote',str(api),'-I',str(api.parent),'-I',str(root/'MacroPad'),str(p/'check.cpp'),str(p/'String.cpp'),str(api.parent/'test/src/itoa.cpp'),str(api.parent/'test/src/dtostrf.cpp'),'-o',str(p/'check')],check=True)
 subprocess.run([str(p/'check')],check=True)
