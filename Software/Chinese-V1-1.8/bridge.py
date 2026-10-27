"""Portable configuration service; native USB automation on Apple Silicon macOS."""
import sys,platform,hashlib,struct
from pathlib import Path
ROOT=Path(__file__).resolve().parent
NATIVE_USB = sys.platform=='darwin' and platform.machine()=='arm64'
def validate_uf2(data):
    if not data or len(data)%512 or len(data)>32*1024*1024: raise ValueError('UF2 文件大小无效')
    count = len(data)//512; seen=set(); addresses=set()
    for offset in range(0,len(data),512):
        a,b,flags,address,size,index,total,family=struct.unpack_from('<8I',data,offset)
        end=struct.unpack_from('<I',data,offset+508)[0]
        if (a,b,end)!=(0x0A324655,0x9E5D5157,0x0AB16F30) or flags != 0x2000 or family!=0xE48BFF56:
            raise ValueError('需要包含 RP2040 family ID 的 Flash UF2 文件')
        if size!=256 or address%256 or not 0x10000000<=address<0x11000000 or address+size>0x11000000 or total!=count or index>=count or index in seen or address in addresses:
            raise ValueError('UF2 地址、分块数量或顺序标识无效')
        seen.add(index); addresses.add(address)
    return hashlib.sha256(data).hexdigest()

if NATIVE_USB:
    from bridge_macos import *
else:
    def status(): return dict(state='manual',message='此电脑使用浏览器USB与目录选择功能，或导出XML手工复制到SD卡。')
    def unsupported(*args,**kwargs): raise RuntimeError('本机未启用原生USB桥接，请使用浏览器连接、目录写入或手动复制流程')
    flash=sd_operation=resume_device=command=unsupported
