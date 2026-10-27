"""Compile the production SD transaction with a fault-injecting in-memory FAT stub."""
from pathlib import Path
import subprocess
import sys
import tempfile
root=Path(__file__).resolve().parents[1];api=Path(sys.argv[1]).resolve()
source=(root/'firmware/MacroPad/menu.ino').read_text()
transaction=source[source.index('bool persistSettingsXML('):source.index('bool saveMenuSettings(')]
header=r'''
#include <cassert>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <iostream>
#include "String.h"
#include "Common.h"
using namespace arduino;
template<class T>T min(T a,T b){return a<b?a:b;}
enum {O_RDONLY=1,O_WRONLY=2,O_CREAT=4,O_TRUNC=8};
struct Faults{bool write=false,sync=false,read=false,renameOld=false,renameNew=false,restore=false;}fault;
struct FsFile {
 std::shared_ptr<std::string> data;size_t position=0;
 explicit operator bool()const{return bool(data);}
 size_t write(const uint8_t *value,size_t count){if(fault.write)count/=2;data->assign((const char*)value,count);return count;}
 bool sync(){return !fault.sync;}
 void close(){data.reset();}
 size_t size(){return data->size();}
 int read(uint8_t *out,size_t count){count=min(count,data->size()-position);memcpy(out,data->data()+position,count);if(fault.read&&count)out[0]^=1;position+=count;return count;}
};
struct FakeSD {
 std::map<std::string,std::shared_ptr<std::string>> files;
 FsFile open(const char *name,int flags){std::string key=name;if(flags&O_CREAT)files[key]=std::make_shared<std::string>();auto it=files.find(key);return it==files.end()?FsFile{}:FsFile{it->second};}
 bool exists(const char *name){return files.count(name);}
 bool remove(const char *name){return files.erase(name);}
 bool rename(const char *a,const char *b){
   if((fault.renameOld&&std::string(a)=="/config.xml")||(fault.renameNew&&std::string(a)=="/config.menu.tmp")||(fault.restore&&std::string(a)=="/config.menu.bak"))return false;
   if(!exists(a)||exists(b))return false;files[b]=files[a];files.erase(a);return true;
 }
 void reset(){files.clear();files["/config.xml"]=std::make_shared<std::string>("original-config");fault=Faults{};}
 std::string get(const char *name){return *files.at(name);}
}SD;
'''
main=r'''
int main(){
 SD.reset();assert(persistSettingsXML("new-config"));assert(SD.get("/config.xml")=="new-config");assert(SD.get("/config.menu.bak")=="original-config");assert(!SD.exists("/config.menu.tmp"));
 assert(persistSettingsXML("third-config"));assert(SD.get("/config.xml")=="third-config");assert(SD.get("/config.menu.bak")=="new-config");
 for(int i=0;i<5;i++){
   SD.reset();switch(i){case 0:fault.write=true;break;case 1:fault.sync=true;break;case 2:fault.read=true;break;case 3:fault.renameOld=true;break;case 4:fault.renameNew=true;break;}
   assert(!persistSettingsXML("replacement"));assert(SD.get("/config.xml")=="original-config");
 }
 SD.reset();fault.renameNew=fault.restore=true;assert(!persistSettingsXML("replacement"));assert(!SD.exists("/config.xml")&&SD.get("/config.menu.bak")=="original-config");
 SD.reset();assert(!persistSettingsXML(""));std::string huge(65537,'x');assert(!persistSettingsXML(String(huge.c_str())));assert(SD.get("/config.xml")=="original-config");
 std::cout<<"PASS: production SD save, complete readback, previous-file backup, short write, sync/read/rename failures, rollback, recoverable backup and size bounds\n";
}
'''
with tempfile.TemporaryDirectory(prefix='haptic-menu-storage-') as temp:
    folder=Path(temp);(folder/'check.cpp').write_text(header+transaction+main)
    subprocess.run(['clang++','-std=c++17','-iquote',str(api),'-I',str(api.parent),str(folder/'check.cpp'),str(api/'String.cpp'),str(api.parent/'test/src/itoa.cpp'),str(api.parent/'test/src/dtostrf.cpp'),'-o',str(folder/'check')],check=True)
    subprocess.run([str(folder/'check')],check=True)
