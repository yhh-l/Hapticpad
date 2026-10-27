import base64,hashlib,json,sys,urllib.request,urllib.error,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import bridge,model
BASE='http://127.0.0.1:8768'
class PortableTests(unittest.TestCase):
 def setUp(self): self.boot=json.load(urllib.request.urlopen(BASE+'/api/bootstrap'))
 def post(self,action,payload,token=True):
  r=urllib.request.Request(BASE+'/api/'+action,json.dumps(payload).encode(),{'Content-Type':'application/json','Origin':BASE,'X-Hapticpad-Token':self.boot['token'] if token else 'bad'})
  return json.load(urllib.request.urlopen(r))
 def test_bootstrap_and_assets(self):
  self.assertEqual(self.boot['toolbox_version'],'1.8');self.assertFalse(self.boot['preview_only']);self.assertEqual(len(self.boot['config']['profiles']),19)
  for asset in ['/','/app.js','/portable.js','/shortcut.js','/style.css','/pet-quotes.zip','/sdr-console.zip','/sdr-guide']:
   self.assertEqual(urllib.request.urlopen(BASE+asset).status,200)
 def test_config_roundtrip(self):
  config=self.boot['config'];config['profiles'][0]['buttons'][0]['label']='测试保存'
  xml=self.post('export',{'config':config})['xml'];actual=self.post('import',{'xml':xml})['config']
  self.assertEqual(actual['profiles'][0]['buttons'][0]['label'],'测试保存');self.assertEqual(len(actual['profiles']),19)
 def test_firmware(self):
  data=urllib.request.urlopen(BASE+'/firmware.uf2').read();expected=json.loads((Path(__file__).resolve().parents[1]/'release/manifest.json').read_text())['sha256']
  self.assertEqual(hashlib.sha256(data).hexdigest(),expected)
  self.assertEqual(self.post('validate-firmware',{'uf2':base64.b64encode(data).decode()})['sha256'],expected)
  with self.assertRaises(urllib.error.HTTPError):self.post('validate-firmware',{'uf2':base64.b64encode(b'bad').decode()})
 def test_auth(self):
  with self.assertRaises(urllib.error.HTTPError) as e:self.post('export',{'config':self.boot['config']},False)
  self.assertEqual(e.exception.code,403)
 def test_manual_platform(self):
  import subprocess
  result=subprocess.run([sys.executable,'-c',"import platform;platform.machine=lambda:'x86_64';import bridge;assert not bridge.NATIVE_USB;assert bridge.status()['state']=='manual'"],capture_output=True,text=True)
  self.assertEqual(result.returncode,0,result.stderr)
if __name__=='__main__':unittest.main()
