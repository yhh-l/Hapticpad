#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>
constexpr int O_RDONLY=0,O_WRONLY=1,O_CREAT=2,O_TRUNC=4;
#include "feiyu_store.h"

struct FakeFS {
  std::map<std::string,std::vector<uint8_t>> files;
  bool directory=false,syncOK=true,renameOK=true,corruptTemp=false;
  int shortWrite=-1;
  struct File {
    FakeFS *fs=nullptr;std::string path;unsigned position=0;bool valid=false;
    explicit operator bool()const{return valid;}
    size_t size()const{return valid?fs->files.at(path).size():0;}
    int read(void *out,unsigned n){
      if(!valid)return -1;
      n=std::min(n,unsigned(size()-position));
      memcpy(out,fs->files[path].data()+position,n);
      if(fs->corruptTemp && path=="/feiyu/upload.tmp" && position>=32 && n)static_cast<uint8_t*>(out)[0]^=1;
      position+=n;return int(n);
    }
    size_t write(const uint8_t *data,size_t n){
      if(!valid)return 0;
      if(fs->shortWrite>=0)n=std::min(n,size_t(fs->shortWrite));
      auto &bytes=fs->files[path];bytes.resize(std::max(bytes.size(),position+n));
      memcpy(bytes.data()+position,data,n);position+=n;return n;
    }
    bool seekSet(unsigned n){if(!valid || n>size())return false;position=n;return true;}
    bool sync(){return fs->syncOK;}
    void close(){valid=false;}
  };
  File open(const char *path,int flags){
    if(flags&O_CREAT)files[path];
    if(!files.count(path))return {};
    if(flags&O_TRUNC)files[path].clear();
    return {this,path,0,true};
  }
  bool exists(const char *path){return std::string(path)=="/feiyu"?directory:files.count(path)>0;}
  bool mkdir(const char *){directory=true;return true;}
  bool remove(const char *path){return files.erase(path)>0;}
  bool rename(const char *from,const char *to){
    if(!renameOK || !files.count(from) || files.count(to))return false;
    files[to]=files[from];files.erase(from);return true;
  }
};

uint32_t checksum(const std::string &s){return feiyuCRC(0xffffffffu,reinterpret_cast<const uint8_t*>(s.data()),s.size())^0xffffffffu;}
void send(FeiyuStore<FakeFS> &store,const std::string &text){
  for(unsigned i=0;i<text.size();i+=128)assert(store.write(i,reinterpret_cast<const uint8_t*>(text.data()+i),std::min(128u,unsigned(text.size()-i))));
}
std::string read(FeiyuStore<FakeFS> &store){
  std::string text;uint8_t bytes[128];
  while(text.size()<store.size){int n=store.read(text.size(),bytes,sizeof(bytes));assert(n>0);text.append(reinterpret_cast<char*>(bytes),n);}
  assert(checksum(text)==store.crc);return text;
}
int main(){
  assert(checksum("123456789")==0xcbf43926u);
  FakeFS fs;FeiyuStore<FakeFS> store(fs);store.begin();assert(store.generation==0);
  const std::string first="{\"pet\":\"大肥鱼\",\"memory\":\"first\"}";
  assert(store.start(first.size(),checksum(first),0));send(store,first);assert(store.commit());
  assert(store.generation==1 && read(store)==first);
  assert(!store.start(first.size(),checksum(first),0));assert(std::string(store.error())=="STALE");
  assert(!store.start(0,0,1));assert(!store.start(store.MAX_BYTES+1,0,1));
  assert(store.start(first.size(),checksum(first),1));assert(!store.commit());
  assert(!store.write(1,reinterpret_cast<const uint8_t*>(first.data()),1));
  // Interrupted upload, including a whole valid but uncommitted temp record.
  send(store,first);FeiyuStore<FakeFS> interrupted(fs);interrupted.begin();assert(interrupted.generation==1&&read(interrupted)==first);store.abort();
  for(unsigned mode=0;mode<4;++mode){
    assert(store.start(first.size(),checksum(first),1));send(store,first);
    fs.syncOK=mode!=0;fs.renameOK=mode!=1;fs.corruptTemp=mode==2;
    if(mode==3)fs.files["/feiyu/upload.tmp"].back()^=1;
    assert(!store.commit());fs.syncOK=fs.renameOK=true;fs.corruptTemp=false;
    FeiyuStore<FakeFS> reopened(fs);reopened.begin();assert(reopened.generation==1&&read(reopened)==first);
  }
  assert(store.start(first.size(),checksum(first),1));fs.shortWrite=1;assert(!store.write(0,reinterpret_cast<const uint8_t*>(first.data()),4));fs.shortWrite=-1;
  assert(store.generation==1&&read(store)==first);
  const std::string second(700,'b');assert(store.start(second.size(),checksum(second),1));send(store,second);assert(store.commit());assert(store.generation==2&&read(store)==second);
  // Corrupted newest payload falls back to the preceding verified generation.
  fs.files["/feiyu/memory1.fym"].back()^=1;FeiyuStore<FakeFS> recovered(fs);recovered.begin();assert(recovered.generation==1&&read(recovered)==first);
  // Header corruption cannot forge a higher generation.
  fs.files["/feiyu/memory1.fym"][8]^=64;FeiyuStore<FakeFS> headerRecovery(fs);headerRecovery.begin();assert(headerRecovery.generation==1);
  FeiyuSaveGesture gesture;
  assert(gesture.update(7,0,true,1000)==1);assert(gesture.update(7,999,true,1000)==1);
  assert(gesture.update(7,1000,true,1000)==2);assert(gesture.update(7,3000,true,1000)==1);
  assert(gesture.update(3,3001,true,1000)==1);assert(gesture.update(0,3002,true,1000)==1);
  assert(gesture.update(1,3003,true,1000)==0);
  assert(gesture.update(7,4000,true,1000)==1);assert(gesture.update(15,4100,true,1000)==1);
  assert(gesture.update(7,6000,true,1000)==1);assert(gesture.update(0,6100,true,1000)==1);
  assert(gesture.update(7,7000,false,1000)==0);
  std::cout<<"PASS: CRC, two-slot recovery, stale writes, interrupted upload, short writes, sync/rename/readback failures, header corruption, 123 hold and release isolation\n";
}
