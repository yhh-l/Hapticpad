#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <utility>

inline uint32_t feiyuCRC(uint32_t state, const uint8_t *bytes, size_t count) {
  while (count--) {
    state ^= *bytes++;
    for (unsigned bit=0;bit<8;++bit) state=(state>>1)^(0xedb88320u & (0u-(state&1u)));
  }
  return state;
}

// Capture only the complete gesture. Existing subset shortcuts retain their
// normal chord window; assemble 1+2+3 together before that window expires.
struct FeiyuSaveGesture {
  bool active=false, fired=false, cancelled=false;
  uint32_t since=0;
  int update(uint8_t buttons,uint32_t now,bool enabled,uint16_t holdMs) {
    if (!enabled) { active=fired=cancelled=false;return 0; }
    if (!active) {
      if (buttons!=7) return 0;
      active=true;fired=cancelled=false;since=now;
    }
    if (!buttons) { active=fired=cancelled=false;return 1; }
    if (buttons!=7) cancelled=true;
    if (!fired && !cancelled && uint32_t(now-since)>=holdMs) {fired=true;return 2;}
    return 1;
  }
};

// Two complete, independently checksummed records. An interrupted upload or
// inactive-slot replacement never intentionally removes the active record.
// FAT and the SD controller still require stable power for reliable writes.
template<class Filesystem> class FeiyuStore {
  using File=decltype(std::declval<Filesystem&>().open("",0));
  struct Header {
    uint32_t magic,version,generation,size,crc,headerCRC,reserved0,reserved1;
  };
  static_assert(sizeof(Header)==32,"Record header must be 32 bytes");
  Filesystem &fs;
  File upload;
  Header pending={};
  uint32_t received=0,running=0xffffffffu;
  int active=-1;
  const char *failure="NONE";
  const char *slotPath(int slot)const{return slot?"/feiyu/memory1.fym":"/feiyu/memory0.fym";}
  static const char *tempPath(){return "/feiyu/upload.tmp";}
  bool fail(const char *why){failure=why;return false;}
  bool inspect(const char *path,Header &h) {
    File file=fs.open(path,O_RDONLY);
    if (!file) return false;
    bool valid=file.read(&h,sizeof(h))==int(sizeof(h)) && h.magic==0x314d5946u &&
      h.version==1 && h.generation>0 && h.size>0 && h.size<=MAX_BYTES &&
      h.reserved0==0 && h.reserved1==0 && file.size()==sizeof(h)+h.size &&
      (feiyuCRC(0xffffffffu,reinterpret_cast<const uint8_t*>(&h),20)^0xffffffffu)==h.headerCRC;
    uint8_t buffer[256];uint32_t state=0xffffffffu,left=valid?h.size:0;
    while(left && valid){
      unsigned n=left<sizeof(buffer)?left:sizeof(buffer);
      if(file.read(buffer,n)!=int(n)){valid=false;break;}
      state=feiyuCRC(state,buffer,n);left-=n;
    }
    file.close();return valid && (state^0xffffffffu)==h.crc;
  }
public:
  static constexpr uint32_t MAX_BYTES=262144;
  uint32_t generation=0,size=0,crc=0;
  bool writing=false;
  explicit FeiyuStore(Filesystem &filesystem):fs(filesystem){}
  const char *error()const{return failure;}
  uint32_t offset()const{return received;}
  void begin() {
    Header a={},b={};bool first=inspect(slotPath(0),a),second=inspect(slotPath(1),b);
    active=second && (!first || b.generation>a.generation)?1:(first?0:-1);
    if(active>=0){const Header &h=active?b:a;generation=h.generation;size=h.size;crc=h.crc;}
    else generation=size=crc=0;
  }
  void abort() {
    upload.close();writing=false;received=0;
    if(fs.exists(tempPath()))fs.remove(tempPath());
  }
  bool start(uint32_t length,uint32_t checksum,uint32_t expected) {
    failure="NONE";
    if(writing)return fail("BUSY");
    if(expected!=generation)return fail("STALE");
    if(generation==0xffffffffu || !length || length>MAX_BYTES)return fail("LIMIT");
    if(!fs.exists("/feiyu") && !fs.mkdir("/feiyu"))return fail("MKDIR");
    if(fs.exists(tempPath()) && !fs.remove(tempPath()))return fail("TEMP");
    pending={0x314d5946u,1,generation+1,length,checksum,0,0,0};
    pending.headerCRC=feiyuCRC(0xffffffffu,reinterpret_cast<const uint8_t*>(&pending),20)^0xffffffffu;
    upload=fs.open(tempPath(),O_WRONLY|O_CREAT|O_TRUNC);
    if(!upload)return fail("OPEN");
    if(upload.write(reinterpret_cast<const uint8_t*>(&pending),sizeof(pending))!=sizeof(pending)){abort();return fail("WRITE");}
    writing=true;received=0;running=0xffffffffu;return true;
  }
  bool write(uint32_t position,const uint8_t *bytes,unsigned count) {
    if(!writing)return fail("NOUPLOAD");
    if(position!=received || !count || count>128 || count>pending.size-received)return fail("OFFSET");
    if(upload.write(bytes,count)!=count){abort();return fail("WRITE");}
    running=feiyuCRC(running,bytes,count);received+=count;return true;
  }
  bool commit() {
    if(!writing)return fail("NOUPLOAD");
    if(received!=pending.size)return fail("INCOMPLETE");
    if((running^0xffffffffu)!=pending.crc){abort();return fail("CRC");}
    if(!upload.sync()){abort();return fail("SYNC");}
    upload.close();Header verified={};
    if(!inspect(tempPath(),verified)){abort();return fail("VERIFY");}
    int next=active==0?1:0;const char *target=slotPath(next);
    if(fs.exists(target) && !fs.remove(target)){abort();return fail("REMOVE");}
    if(!fs.rename(tempPath(),target)){abort();return fail("RENAME");}
    writing=false;
    if(!inspect(target,verified))return fail("VERIFY");
    active=next;generation=verified.generation;size=verified.size;crc=verified.crc;
    return true;
  }
  int read(uint32_t position,uint8_t *out,unsigned capacity) {
    if(active<0)return -1;
    if(position>size)return -1;
    unsigned count=size-position<capacity?size-position:capacity;
    File file=fs.open(slotPath(active),O_RDONLY);
    if(!file || !file.seekSet(sizeof(Header)+position)){file.close();return -1;}
    int n=file.read(out,count);file.close();return n==int(count)?n:-1;
  }
};
