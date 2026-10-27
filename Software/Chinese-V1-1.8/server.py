"""Loopback-only web UI and serialized hardware jobs; no third-party Python deps."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit
import argparse
import base64
import json
import secrets
import threading
import webbrowser
import bridge
import model

ROOT=Path(__file__).resolve().parent
PREVIEW_ONLY=False
TOKEN=secrets.token_urlsafe(32)
LOCK=threading.Lock()
JOB_LOCK=threading.Lock()
JOB={'id':0,'state':'idle','logs':[]}

def job_snapshot():
    with JOB_LOCK: return dict(JOB,logs=list(JOB['logs']))

def launch(action, payload):
    if PREVIEW_ONLY and action != 'status': raise ValueError('本地无极候选仅供编辑与导出，已禁止刷机、读写SD及电机操作')
    if not LOCK.acquire(blocking=False): raise ValueError('设备正在处理另一项操作，请等待完成')
    global JOB
    with JOB_LOCK:
        JOB={'id':JOB['id']+1,'state':'running','logs':[]}
        job_id=JOB['id']
    def log(line):
        with JOB_LOCK: JOB['logs'].append(line)
    def work():
        try:
            if action=='flash':
                path=ROOT/'release/Hapticpad_V1_CN.uf2'
                if payload.get('uf2'):
                    data=base64.b64decode(payload['uf2'],validate=True); bridge.validate_uf2(data)
                    path=ROOT/'release/custom-upload.uf2'; path.write_bytes(data)
                result=bridge.flash(path,log)
            elif action in ('read','write'):
                result=bridge.sd_operation(payload.get('config') if action=='write' else None,log)
            elif action=='motor':
                enabled=payload.get('enabled')
                if not isinstance(enabled,bool): raise ValueError('开关参数错误')
                result={'response':bridge.command('HP MOTOR '+str(int(enabled)))}
                log('已发送临时力反馈开关；保存到 SD 后才会在重启后保留')
            elif action=='status': result=bridge.status()
            elif action=='resume': result=bridge.resume_device(log)
            else: raise ValueError('未知操作')
            with JOB_LOCK: JOB.update(state='done',result=result)
        except Exception as e:
            log(str(e))
            with JOB_LOCK: JOB.update(state='error',error=str(e))
        finally: LOCK.release()
    threading.Thread(target=work,daemon=True).start()
    return {'id':job_id}

class Handler(BaseHTTPRequestHandler):
    def log_message(self,*args): pass
    def reply(self,status,data,kind='application/json; charset=utf-8'):
        raw=json.dumps(data,ensure_ascii=False).encode() if not isinstance(data,bytes) else data
        self.send_response(status); self.send_header('Content-Type',kind)
        self.send_header('Content-Length',str(len(raw))); self.send_header('Cache-Control','no-store')
        self.send_header('X-Content-Type-Options','nosniff'); self.send_header('Referrer-Policy','no-referrer')
        self.send_header('Content-Security-Policy',"default-src 'self'; script-src 'self'; style-src 'self'; connect-src 'self'; img-src 'self' data:; frame-ancestors 'none'; base-uri 'none'; form-action 'self'")
        self.end_headers(); self.wfile.write(raw)
    def host_ok(self):
        return self.headers.get('Host')==f'127.0.0.1:{self.server.server_port}'
    def do_GET(self):
        if not self.host_ok(): return self.reply(403,{'error':'Host 不匹配'})
        path=urlsplit(self.path).path
        if path=='/api/bootstrap':
            config=model.from_xml((ROOT/'release/installed-config.xml').read_bytes())
            return self.reply(200,dict(token=TOKEN,config=config,config_templates={},feedback_defaults=model.FEEDBACK_DEFAULTS,feedback_presets=model.FEEDBACK_PRESETS,flywheel_defaults=model.FLYWHEEL_DEFAULTS,flywheel_presets=model.FLYWHEEL_PRESETS,preview_only=PREVIEW_ONLY,native_usb=bridge.NATIVE_USB,toolbox_version="1.8"))
        if path=='/api/job': return self.reply(200,job_snapshot())
        files={'/':'index.html','/index.html':'index.html','/app.js':'app.js','/portable.js':'portable.js','/shortcut.js':'shortcut.js','/style.css':'style.css','/pixel-pets.gif':'pixel-pets.gif'}
        if path in files:
            p=ROOT/'web'/files[path]
            return self.reply(200,p.read_bytes(),{'.html':'text/html; charset=utf-8','.js':'text/javascript; charset=utf-8','.css':'text/css; charset=utf-8','.gif':'image/gif'}[p.suffix])
        if path=='/firmware.uf2': return self.reply(200,(ROOT/'release/Hapticpad_V1_CN.uf2').read_bytes(),'application/octet-stream')
        if path=='/pet-quotes.zip': return self.reply(200,(ROOT/'release/像素宠物台词库.zip').read_bytes(),'application/zip')
        if path=='/sdr-console.zip': return self.reply(200,(ROOT/'release/Windows_SDR控制台.zip').read_bytes(),'application/zip')
        if path=='/sdr-guide': return self.reply(200,(ROOT/'release/SDR使用说明.html').read_bytes(),'text/html; charset=utf-8')
        if path=='/sdr-guide.css': return self.reply(200,(ROOT/'release/SDR说明.css').read_bytes(),'text/css; charset=utf-8')
        return self.reply(404,{'error':'页面不存在'})
    def do_POST(self):
        origin=f'http://127.0.0.1:{self.server.server_port}'
        if not self.host_ok() or self.headers.get('Origin')!=origin or self.headers.get('X-Hapticpad-Token')!=TOKEN:
            return self.reply(403,{'error':'请从本机工具箱网页发起操作'})
        try:
            size=int(self.headers.get('Content-Length','0'))
            if size<1 or size>45*1024*1024: raise ValueError('请求大小无效')
            if self.headers.get('Content-Type','').split(';')[0]!='application/json': raise ValueError('需要 JSON 请求')
            payload=json.loads(self.rfile.read(size))
            if not isinstance(payload,dict): raise ValueError('请求格式错误')
            action=urlsplit(self.path).path.removeprefix('/api/')
            if action=='validate-firmware': return self.reply(200,{'sha256':bridge.validate_uf2(base64.b64decode(payload['uf2'],validate=True))})
            if action=='import': return self.reply(200,{'config':model.from_xml(payload['xml'])})
            if action=='export': return self.reply(200,{'xml':model.to_xml(payload['config']).decode()})
            if action=='sdr-config': return self.reply(200,{'config':model.add_sdr_profile(payload['config'])})
            if action not in ('flash','read','write','motor','status','resume'): return self.reply(404,{'error':'接口不存在'})
            if PREVIEW_ONLY and action != 'status': raise ValueError('本地无极候选仅供编辑与导出，已禁止刷机、读写SD及电机操作')
            if action=='write': payload['config']=model.validate(payload['config'])
            if action=='flash' and payload.get('uf2'): bridge.validate_uf2(base64.b64decode(payload['uf2'],validate=True))
            return self.reply(202,launch(action,payload))
        except Exception as e: return self.reply(400,{'error':str(e)})

def main():
    parser=argparse.ArgumentParser(); parser.add_argument('--open',action='store_true'); parser.add_argument('--port',type=int,default=8768); args=parser.parse_args()
    try: server=ThreadingHTTPServer(('127.0.0.1',args.port),Handler)
    except OSError:
        import urllib.request
        url=f'http://127.0.0.1:{args.port}/api/bootstrap'
        try:
            existing=json.load(urllib.request.urlopen(url,timeout=2))
            if existing.get('toolbox_version')!='1.8' or existing.get('preview_only') is not False: raise ValueError()
        except Exception: raise SystemExit('端口被其他版本占用，请关闭旧工具箱，或使用 python3 server.py --port 8767 --open')
        if args.open: webbrowser.open(f'http://127.0.0.1:{args.port}/')
        print('工具箱已在运行'); return
    print(f'工具箱：http://127.0.0.1:{server.server_port}/',flush=True)
    if args.open: webbrowser.open(f'http://127.0.0.1:{server.server_port}/')
    try: server.serve_forever()
    except KeyboardInterrupt: pass
    finally: server.server_close()

if __name__=='__main__': main()
