"""macOS USB/SD bridge. Hardware commands run with bounded timeouts."""
import array
import hashlib
import json
import os
from pathlib import Path
import plistlib
import select
import secrets
import struct
import subprocess
import sys
import termios
import time

ROOT = Path(__file__).resolve().parent
SERIAL = 'E46468C09732292D'
PICOTOOL = ROOT / 'tools/picotool'

def run(args, timeout=15):
    p = subprocess.run([str(x) for x in args], capture_output=True, timeout=timeout)
    if p.returncode: raise RuntimeError((p.stderr or p.stdout).decode(errors='replace').strip() or f'命令失败：{p.returncode}')
    return p.stdout

def descendants(node):
    yield node
    for child in node.get('IORegistryEntryChildren', []): yield from descendants(child)

def usb_nodes():
    if sys.platform != 'darwin': raise RuntimeError('此工具箱的 USB 服务目前支持 macOS；固件中的 Win 配置可用于 Windows')
    # A USB re-enumeration can invalidate an ioreg child mid-snapshot.
    for attempt in range(3):
        try:
            raw = run(['ioreg','-a','-l','-r','-c','IOUSBHostDevice'])
            break
        except RuntimeError as e:
            if "can't obtain child" not in str(e): raise
            if attempt==2: return []
            time.sleep(.1)
    if not raw.strip(): return []
    roots = plistlib.loads(raw)
    return [n for root in roots for n in descendants(root) if n.get('USB Serial Number') == SERIAL and n.get('idVendor') == 0x239a and n.get('idProduct') == 0xcafe]

def serial_port():
    ports = {n['IOCalloutDevice'] for root in usb_nodes() for n in descendants(root) if n.get('IOCalloutDevice')}
    if len(ports) != 1: raise RuntimeError('未找到此 Pico 的唯一串口，请连接设备后重试')
    return ports.pop()

def command(text):
    # Opening a stale macOS serial node can hang: isolate even open() in a child.
    return run([sys.executable, __file__, '--serial', serial_port(), text], 7).decode().strip()

def serial_child(port, text):
    import fcntl
    fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    try:
        a = termios.tcgetattr(fd); a[0] = a[1] = a[3] = 0
        a[2] = termios.CS8 | termios.CLOCAL | termios.CREAD
        a[4] = a[5] = termios.B115200; termios.tcsetattr(fd, termios.TCSANOW, a)
        fcntl.ioctl(fd, termios.TIOCMBIS, array.array('i',[termios.TIOCM_DTR]))
        # Let USB CDC DTR settle; remove any old response before sending.
        time.sleep(.12)
        while select.select([fd],[],[],0)[0]:
            if not os.read(fd,4096): break
        os.write(fd, (text+'\n').encode()); data = b''; until = time.monotonic()+4
        prefix = b'HP INFO ' if text == 'HP INFO' else b'HP OK '
        while time.monotonic() < until:
            if select.select([fd],[],[],.15)[0]:
                chunk = os.read(fd,4096)
                if not chunk: break
                data += chunk
                for line in data.splitlines():
                    if line.startswith(prefix) and b'\n' in data: print(line.decode()); return
        raise RuntimeError('设备未响应；若仍是旧固件，请先刷入随附中文固件')
    finally: os.close(fd)

def boot_available():
    try: run([PICOTOOL,'info','--ser',SERIAL],5); return True
    except (RuntimeError,subprocess.TimeoutExpired): return False

def software_boot():
    try: command('HP BOOT')
    except Exception:
        run([sys.executable, ROOT/'tools/software_boot.py'],22)
    until = time.monotonic()+12
    while time.monotonic()<until:
        if boot_available(): return
        time.sleep(.5)
    raise RuntimeError('未进入刷机模式。可拔插 USB 后重试；首次刷入的旧固件需支持 USB 软件复位')

def status():
    if usb_nodes():
        try:
            response = command('HP INFO')
            return dict(state='ready', serial=SERIAL, info=json.loads(response.removeprefix('HP INFO ')))
        except Exception as e: return dict(state='legacy',serial=SERIAL,message=str(e))
    if boot_available(): return dict(state='boot',serial=SERIAL,message='已进入 BOOTSEL，可直接刷入')
    return dict(state='disconnected',message='设备未连接；可以先编辑、导出配置')

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

def backup_dir():
    folder=ROOT/'backups'/(time.strftime('%Y%m%d_%H%M%S')+'_'+secrets.token_hex(2))
    folder.mkdir(parents=True,exist_ok=False); return folder

def flash(path, log):
    data=Path(path).read_bytes(); digest=validate_uf2(data)
    log('校验 UF2：'+digest)
    if not boot_available():
        log('正在通过 USB 软件进入刷机模式…'); software_boot()
    folder=backup_dir(); log('备份设备当前完整 16MB Flash…')
    run([PICOTOOL,'save','-r','0x10000000','0x11000000',folder/'flash-before.bin','--ser',SERIAL],120)
    backup=folder/'flash-before.bin'
    if backup.stat().st_size != 16777216:
        raise RuntimeError('Flash 备份大小不完整，已停止刷写')
    backup_digest=hashlib.sha256(backup.read_bytes()).hexdigest()
    log('写入固件…'); run([PICOTOOL,'load','-v',path,'--ser',SERIAL],90)
    log('从设备回读校验…'); run([PICOTOOL,'verify',path,'--ser',SERIAL],60)
    (folder/'flash.json').write_text(json.dumps(dict(sha256=digest,backup_sha256=backup_digest,serial=SERIAL,source=str(path)),indent=2))
    run([PICOTOOL,'reboot','--ser',SERIAL]); log('已写入并回读校验，等待设备重启…')
    until=time.monotonic()+18
    while time.monotonic()<until:
        try:
            s=status()
            if s['state']=='ready': return dict(sha256=digest, backup=str(folder),device=s)
        except Exception: pass
        time.sleep(1)
    return dict(sha256=digest,backup=str(folder),message='固件回读校验通过；尚未收到新固件回应，请检查屏幕并刷新连接')

def sd_volume():
    # Never select an arbitrary removable drive by volume name.
    disks={n['BSD Name'] for root in usb_nodes() for n in descendants(root) if n.get('BSD Name') and n.get('IOObjectClass')=='IOMedia'}
    for disk in sorted(disks, key=len, reverse=True):
        try:
            info=plistlib.loads(run(['diskutil','info','-plist','/dev/'+disk]))
        except RuntimeError as e:
            # The old registry snapshot can outlive this disk during USB mode switching.
            if 'Could not find disk: /dev/'+disk in str(e): continue
            raise
        mount=info.get('MountPoint')
        if mount and info.get('FilesystemType')=='msdos':
            whole=info.get('ParentWholeDisk',disk)
            return Path(mount), '/dev/'+whole
    return None

def resume_device(log):
    volume=sd_volume()
    if volume:
        log('安全弹出目标设备的 SD 卡…'); run(['diskutil','eject',volume[1]],25)
    try:
        command('HP REBOOT'); log('等待设备退出配置模式并恢复键盘和鼠标…')
    except Exception as e:
        return dict(resumed=False,message='未能请求重启，请拔插 USB 恢复使用：'+str(e))
    until=time.monotonic()+18
    while time.monotonic()<until:
        device=status(); info=device.get('info',{})
        if device['state']=='ready' and not info.get('disk_mode',True):
            if info.get('keyboard_ready',False) and info.get('mouse_ready',False):
                log('已确认退出 U盘模式，键盘和鼠标已恢复')
                return dict(resumed=True,device=device)
        time.sleep(.5)
    return dict(resumed=False,message='SD 已安全弹出，但未确认键盘和鼠标恢复；请拔插 USB 后点击检测设备')

def sd_operation(config, log):
    import model
    data=model.to_xml(config) if config is not None else None
    volume=sd_volume()
    if not volume:
        log('停止力反馈并进入 U盘配置模式…'); command('HP SDMODE')
        log('等待 macOS 挂载 SD 卡，通常需要 10～60 秒…')
        until=time.monotonic()+75
        while time.monotonic()<until:
            volume=sd_volume()
            if volume: break
            time.sleep(1)
    if not volume: raise RuntimeError('SD 卡未挂载，请检查设备屏幕；保留当前编辑内容，可重新连接后重试')
    mount,disk=volume; log('已确认 SD 卡属于目标 Pico：'+str(mount))
    target=mount/'config.xml'; folder=backup_dir()
    old=target.read_bytes() if target.exists() else None
    if old is not None: (folder/'config-before.xml').write_bytes(old)
    for name in ('last.txt','lastProfile'):
        if (mount/name).exists(): (folder/name).write_bytes((mount/name).read_bytes())
    if data is None:
        if old is None: raise RuntimeError('SD 卡没有 config.xml，可点击“写入 SD”初始化')
        result=model.from_xml(old); log('配置已读取，旧配置已备份')
    else:
        # Recheck device identity immediately before modifying files.
        if sd_volume()!=volume: raise RuntimeError('SD 设备在写入前发生变化，已停止')
        temp=mount/'config.cn.tmp'
        with temp.open('wb') as f: f.write(data); f.flush(); os.fsync(f.fileno())
        os.replace(temp,target)
        with (mount/'last.txt').open('wb') as f: f.write(b'0'); f.flush(); os.fsync(f.fileno())
        if target.read_bytes()!=data: raise RuntimeError('SD 配置回读不一致，请勿拔线，重试写入')
        result=model.from_xml(data); log('配置写入并回读核对通过')
    log('安全弹出 SD 卡…'); run(['diskutil','eject',disk],25)
    resumed=resume_device(log)
    if resumed.get('message'): log(resumed['message'])
    return dict(config=result,backup=str(folder),**resumed)

if __name__=='__main__' and sys.argv[1:2]==['--serial']:
    try: serial_child(sys.argv[2],sys.argv[3])
    except Exception as e: print(str(e),file=sys.stderr); sys.exit(1)
