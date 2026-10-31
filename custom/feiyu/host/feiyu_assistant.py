"""Real DSH session control and a paged, read-only OLED view."""
from pathlib import Path
import json
import os
import time
import unicodedata
import urllib.request
from urllib.parse import urlsplit
import webbrowser


def pages_for(text):
    """Three 20-cell lines fit the 128-pixel wqy12 OLED font."""
    lines, current, width = [], '', 0
    for char in str(text).replace('\r', '').replace('\t', '  ')[:12000]:
        if char == '\n':
            lines.append(current); current, width = '', 0
            continue
        if unicodedata.category(char).startswith('C'):
            continue
        # The installed GB2312 font has no emoji: show a visible placeholder.
        try:
            char.encode('gb2312')
        except UnicodeEncodeError:
            char = '?'
        cells = 2 if ord(char) > 127 else 1
        if width + cells > 20:
            lines.append(current); current, width = '', 0
        current += char; width += cells
    if current or not lines:
        lines.append(current or '等待任务')
    pages = ['\n'.join(lines[i:i+3]) for i in range(0, len(lines), 3)]
    assert all(len(page.encode('utf-8')) <= 125 for page in pages)
    return pages[:512]


class Assistant:
    def __init__(self, workspace):
        self.workspace = Path(workspace).resolve()
        self.endpoint_file = Path(os.environ.get('DSH_HOME') or
                                  str(Path(os.environ.get('LOCALAPPDATA', str(Path.home() / '.local/share'))) / 'FeiyuDSH')) / 'feiyu-assistant-endpoint.json'
        self.endpoint = None
        self.state = {}
        self.pages, self.page, self.content = pages_for('1打开任务窗口\n2接着上次做\n3整理并保存'), 0, None
        self.sent_page = None
        self.save_id = None
        self.save_request = None
        self.save_started = 0
        self.error = ''
        self.connection_error = False
        self.handled = []

    def api(self, path, body=None):
        endpoint = json.loads(self.endpoint_file.read_text(encoding='utf-8-sig'))
        parsed = urlsplit(endpoint['url'])
        if (parsed.scheme != 'http' or parsed.hostname != '127.0.0.1' or
                parsed.username or parsed.password or parsed.path or parsed.query or parsed.fragment or
                Path(endpoint['workspace']).resolve() != self.workspace):
            raise RuntimeError('Unexpected assistant endpoint')
        self.endpoint = endpoint
        data = None if body is None else json.dumps(body).encode('utf-8')
        request = urllib.request.Request(endpoint['url'] + path, data=data,
            headers={'Authorization': 'Bearer ' + endpoint['token'], 'Content-Type': 'application/json'})
        # Avoid system proxy settings for this local control link.
        opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
        try:
            with opener.open(request, timeout=3) as response:
                return json.load(response)
        except urllib.error.HTTPError as error:
            try:
                message = json.load(error).get('error', '助手请求失败')
            except (ValueError, OSError):
                message = '助手请求失败'
            raise RuntimeError(message) from None

    def open(self):
        self.api('/state')
        webbrowser.open(self.endpoint['url'] + '/#' + self.endpoint['token'])

    def tick(self, link, status, dirty):
        try:
            self.state = self.api('/state')
            if self.connection_error:
                self.error = ''; self.connection_error = False
            remote = json.loads(link.command('HP AI INFO')[11:])
            if remote.get('protocol') != 1:
                raise RuntimeError('需要新版助手固件')
            action, event_id = remote['action'], remote['event_id']
            if event_id:
                if event_id not in self.handled:
                    self.error = ''
                    try:
                        if action == 'open': self.open()
                        elif action == 'resume': self.api('/resume', {})
                        elif action == 'save' and not status.get('save_pending'): link.command('HP MEM SAVE')
                        elif action == 'next': self.page = min(len(self.pages)-1, self.page+1)
                        elif action == 'previous': self.page = max(0, self.page-1)
                    except (OSError, RuntimeError, ValueError) as error:
                        self.error = str(error)
                    self.handled = (self.handled + [event_id])[-32:]
                if link.command(f'HP AI ACK {event_id}') != 'HP AI OK ACK':
                    raise RuntimeError('键盘操作确认失败')
            if (self.state.get('bridge') or {}).get('saveRequested') and not status.get('save_pending'):
                link.command('HP MEM SAVE')
            self.api('/bridge', {'connected':True, 'generation':status['generation'],
                'dirty':dirty, 'saving':bool(self.save_id), 'error':self.error})
            phase = 'saving' if self.save_id else self.state.get('phase', 'idle')
            if self.error: phase = 'error'
            if phase not in ('idle','thinking','reply','tool','done','cancelled','error','saving'):
                phase = 'error'
            if link.command('HP AI STATE ' + phase) != 'HP AI OK STATE':
                raise RuntimeError('助手状态未同步')
            text = self.error or self.state.get('text') or self.state.get('prompt') or '1打开任务窗口\n2接着上次做\n3整理并保存'
            if phase == 'tool': text = '正在执行：' + self.state.get('tool','工具') + '\n' + text
            if self.content != text:
                self.content = text; self.pages = pages_for(text)
                self.page = min(self.page, len(self.pages)-1)
            payload = (self.page, len(self.pages), self.pages[self.page])
            if payload != self.sent_page or remote['page'] != self.page:
                response = link.command(f'HP AI PAGE {self.page} {len(self.pages)} ' + payload[2].encode('utf-8').hex())
                if response != 'HP AI OK PAGE':
                    raise RuntimeError('屏幕回复同步失败')
                self.sent_page = payload
        except (OSError, RuntimeError, ValueError, KeyError, TypeError) as error:
            self.connection_error = True
            self.error = '助手未连接，请打开桌面大肥鱼入口' if isinstance(error, OSError) else str(error)
            self.sent_page = None
            # Stop refreshing the online lease. Firmware expires it automatically.

    def prepare_save(self, request):
        if self.save_request != request:
            self.error = ''
            result = self.api('/checkpoint', {})
            self.save_id = result['id']; self.save_request = request; self.save_started = time.monotonic()
        if time.monotonic()-self.save_started > 180:
            raise RuntimeError('整理记忆超时，本次没有确认保存，请重试')
        job = self.api('/job', {'id':self.save_id})
        if job['status'] == 'failed':
            raise RuntimeError(job.get('error','记忆整理失败'))
        return job['status'] == 'completed'

    def saved(self):
        self.save_id = None; self.save_request = None; self.error = ''

    def failed(self, error):
        self.save_id = None; self.save_request = None; self.error = str(error)

    def disconnected(self):
        self.sent_page = None
        self.handled = []
        try:
            self.api('/bridge', {'connected':False,'generation':0,'dirty':False,'saving':False,'error':'键盘已断开'})
        except (OSError, RuntimeError, ValueError):
            pass
