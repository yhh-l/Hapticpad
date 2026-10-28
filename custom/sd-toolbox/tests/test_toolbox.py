import copy
import http.client
import json
from pathlib import Path
import plistlib
import struct
import sys
import tempfile
import threading
import unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import bridge_macos as bridge
import model
import server

ROOT=Path(__file__).resolve().parents[1]
class ConfigTests(unittest.TestCase):
    def setUp(self): self.config=model.from_xml((ROOT/'release/config.xml').read_bytes())
    def test_knob_led_roundtrip_and_limits(self):
        p=self.config['profiles'][0]
        self.assertEqual((p['led_direction'],p['led_offset']),(1,0))
        p.update(led='Knob',led_direction=-1,led_offset=19,secondary=[0,4,12])
        self.assertEqual(model.from_xml(model.to_xml(self.config)),self.config)
        p['led']='KnobTrail'
        self.assertEqual(model.from_xml(model.to_xml(self.config)),self.config)
        for key,value in [('led_direction',0),('led_direction',True),('led_direction',2),('led_offset',20),('led_offset',-1)]:
            c=copy.deepcopy(self.config);c['profiles'][0][key]=value
            with self.assertRaises(ValueError):model.to_xml(c)
    def test_display_scenes_roundtrip(self):
        self.assertEqual(self.config['profiles'][0]['display_scene'],'Dog')
        for scene in ('Dog','Person','Text','Off'):
            self.config['profiles'][0]['display_scene']=scene
            self.assertEqual(model.from_xml(model.to_xml(self.config)),self.config)
        self.config['profiles'][0]['display_scene']='invalid'
        with self.assertRaises(ValueError):model.to_xml(self.config)
    def test_display_text_optional_roundtrip_and_bounds(self):
        self.assertEqual(self.config['profiles'][0]['display_text'], '')
        p=self.config['profiles'][0]
        original=copy.deepcopy(p['buttons'])
        for text in ('', '专注工作', '频率<&"', '中'*10+'M'):
            p['display_text']=text
            loaded=model.from_xml(model.to_xml(self.config))
            self.assertEqual(loaded,self.config)
            self.assertEqual(loaded['profiles'][0]['buttons'],original)
        for text in ('中'*11, 'x'*32, 'a\nb', None):
            p['display_text']=text
            with self.assertRaises(ValueError):model.to_xml(self.config)
    def test_mac_win_roundtrip(self):
        c=model.from_xml(model.to_xml(self.config)); self.assertEqual(c,self.config)
        self.assertFalse(c['motor_enabled'])
        for p,mod in zip(c['profiles'],(91,17)):
            self.assertEqual([b['label'] for b in p['buttons']],['复制','粘贴','剪切','全选','撤销','退格'])
            self.assertEqual([b['actions'] for b in p['buttons']],[[[0,mod],[0,k],[0,0]] for k in (67,86,88,65,90)]+[[[0,8],[0,0],[0,0]]])
    def test_reject_invalid_or_overlong(self):
        for field,value in [('name','长'*11),('mode','bad'),('direction',0),('brightness',256)]:
            c=copy.deepcopy(self.config);c['profiles'][0][field]=value
            with self.assertRaises(ValueError): model.to_xml(c)
        for data in (b'<!DOCTYPE x><Configuration/>',b'x'*65537,b'<wrong/>'):
            with self.assertRaises((ValueError,model.ET.ParseError,model.expat.ExpatError)): model.from_xml(data)
    def test_nonzero_delays_custom_chinese_and_xml_escaping(self):
        p=self.config['profiles'][0];p['name']='Mac<&"';p['buttons'][0]={'label':'重做','actions':[[0,91],[30,16],[100,90]]}
        self.assertEqual(model.from_xml(model.to_xml(self.config)),self.config)
    def test_uf2_validation(self):
        data=(ROOT/'release/Hapticpad_V1_CN.uf2').read_bytes();self.assertEqual(len(bridge.validate_uf2(data)),64)
        for offset,value in [(28,0),(12,0x20000000),(24,100000),(508,0)]:
            bad=bytearray(data);struct.pack_into('<I',bad,offset,value)
            with self.assertRaises(ValueError): bridge.validate_uf2(bad)
        with self.assertRaises(ValueError): bridge.validate_uf2(data+data[:512])

    def test_feedback_presets_survive_sd_roundtrip(self):
        for preset in model.FEEDBACK_PRESETS.values():
            c=copy.deepcopy(self.config);p=c['profiles'][0]
            p['mode']=preset['wheel_mode'];p['feedback']=copy.deepcopy(preset['feedback'])
            self.assertEqual(model.from_xml(model.to_xml(c)),c)
    def test_ready_to_use_config_collections(self):
        packs=model.configuration_templates(self.config)
        self.assertEqual(len(packs['basic']['config']['profiles']),2)
        collection=packs['haptics']['config'];self.assertEqual(len(collection['profiles']),16)
        for pack in packs.values():
            c=pack['config'];self.assertFalse(c['motor_enabled'])
            self.assertEqual(model.from_xml(model.to_xml(c)),c)
            for p in c['profiles']:
                self.assertEqual(p['mode'],'Clicky')
                self.assertEqual(p['buttons'],self.config['profiles'][0 if p['name'].startswith('Mac') else 1]['buttons'])
        self.assertEqual({p['feedback']['mode'] for p in collection['profiles']},{'Follow','Damping','Assist','Free'})
    def test_feedback_limits_and_mode_compatibility(self):
        for key,value in [('voltage',3.1),('voltage',float('nan')),('strength',101),('damping',-1),('assist',101),('detents',0),('assist_ms',601),('speed_rpm',121),('trigger_rpm',31)]:
            c=copy.deepcopy(self.config);c['profiles'][0]['feedback'][key]=value
            with self.assertRaises(ValueError,msg=key):model.to_xml(c)
        c=copy.deepcopy(self.config);c['profiles'][0]['mode']='Twist';c['profiles'][0]['feedback']['mode']='Assist'
        with self.assertRaisesRegex(ValueError,'刻度滚动'):model.to_xml(c)
    def test_sdr_append_preserves_motor_profiles_and_binding_contract(self):
        c=model.configuration_templates(self.config)['haptics']['config'];c['motor_enabled']=True
        updated=model.add_sdr_profile(c)
        self.assertEqual(len(updated['profiles']),17)
        self.assertTrue(updated['motor_enabled'])
        self.assertEqual(updated['profiles'][:-1],c['profiles'])
        p=updated['profiles'][-1]
        self.assertEqual(p['wheel_key'],18)
        self.assertEqual([b['actions'] for b in p['buttons']],[[[0,17],[0,18],[0,112+i]] for i in range(6)])
        self.assertEqual(model.from_xml(model.to_xml(updated)),updated)
        self.assertEqual(model.add_sdr_profile(updated),updated)
        c['profiles']=[copy.deepcopy(c['profiles'][0]) for _ in range(256)]
        with self.assertRaises(ValueError):model.add_sdr_profile(c)
    def test_legacy_config_gets_soft_default_feedback(self):
        xml=model.to_xml(self.config)
        import xml.etree.ElementTree as ET
        root=ET.fromstring(xml)
        for p in root.findall('./Profiles/Profile'):
            for tag in model.FEEDBACK_TAGS.values():p.remove(p.find(tag))
        loaded=model.from_xml(ET.tostring(root))
        self.assertFalse(loaded['motor_enabled'])
        self.assertEqual(loaded['profiles'][0]['feedback'],model.FEEDBACK_DEFAULTS)

class DeviceTests(unittest.TestCase):
    def test_usb_reenumeration_retries_transient_registry_error(self):
        with patch.object(bridge,'run',side_effect=[RuntimeError("ioreg: error: can't obtain child."),plistlib.dumps([])]),patch.object(bridge.time,'sleep'):
            self.assertEqual(bridge.usb_nodes(),[])
        with patch.object(bridge,'run',side_effect=RuntimeError('permission denied')):
            with self.assertRaisesRegex(RuntimeError,'permission denied'):bridge.usb_nodes()
    def test_resume_requires_normal_mode_and_hid_ready(self):
        disk={'state':'ready','info':{'disk_mode':True}}
        normal={'state':'ready','info':{'disk_mode':False,'keyboard_ready':True,'mouse_ready':True}}
        with patch.object(bridge,'sd_volume',return_value=None),patch.object(bridge,'command',return_value='HP OK REBOOT'),patch.object(bridge,'status',side_effect=[disk,normal]),patch.object(bridge.time,'sleep'):
            result=bridge.resume_device(lambda _:None)
        self.assertTrue(result['resumed']);self.assertEqual(result['device'],normal)
        with patch.object(bridge,'sd_volume',return_value=None),patch.object(bridge,'command',return_value='HP OK REBOOT'),patch.object(bridge,'status',return_value=disk),patch.object(bridge.time,'sleep'),patch.object(bridge.time,'monotonic',side_effect=[0,0,30]):
            self.assertFalse(bridge.resume_device(lambda _:None)['resumed'])
        legacy={'state':'ready','info':{'disk_mode':False}}
        with patch.object(bridge,'sd_volume',return_value=None),patch.object(bridge,'command',return_value='HP OK REBOOT'),patch.object(bridge,'status',return_value=legacy),patch.object(bridge.time,'sleep'),patch.object(bridge.time,'monotonic',side_effect=[0,0,30]):
            self.assertFalse(bridge.resume_device(lambda _:None)['resumed'])
    def test_usb_scan_includes_properties_on_descendants(self):
        # ioreg without -l gives properties on matched roots only, leaving
        # serial and disk children present but without their device names.
        tree={'USB Serial Number':bridge.SERIAL,'idVendor':0x239a,'idProduct':0xcafe,
              'IORegistryEntryChildren':[{'IOObjectClass':'IOSerialBSDClient','IOCalloutDevice':'/dev/cu.target'},
                                         {'IOObjectClass':'IOMedia','BSD Name':'disk99s1'}]}
        info={'MountPoint':'/Volumes/test-pico','FilesystemType':'msdos','ParentWholeDisk':'disk99'}
        def registry(args,timeout=15):
            if args[0]=='ioreg':
                value=copy.deepcopy(tree)
                if '-l' not in args:
                    for child in value['IORegistryEntryChildren']:
                        child.pop('IOCalloutDevice',None);child.pop('BSD Name',None)
                return plistlib.dumps([value])
            return plistlib.dumps(info)
        with patch.object(bridge,'run',side_effect=registry):
            self.assertEqual(bridge.serial_port(),'/dev/cu.target')
            self.assertEqual(bridge.sd_volume(),(Path('/Volumes/test-pico'),'/dev/disk99'))
    def test_incomplete_flash_backup_prevents_programming(self):
        with tempfile.TemporaryDirectory() as temp:
            folder=Path(temp)
            def save_only(args, timeout=15):
                self.assertEqual(args[1],'save')
                (folder/'flash-before.bin').write_bytes(b'incomplete')
                return b''
            with patch.object(bridge,'boot_available',return_value=True),patch.object(bridge,'backup_dir',return_value=folder),patch.object(bridge,'run',side_effect=save_only):
                with self.assertRaisesRegex(RuntimeError,'备份大小不完整'):
                    bridge.flash(ROOT/'release/Hapticpad_V1_CN.uf2',lambda _:None)
    def test_no_usb_devices_is_disconnected(self):
        with patch.object(bridge,'run',return_value=b''),patch.object(bridge,'boot_available',return_value=False):
            self.assertEqual(bridge.usb_nodes(),[])
            self.assertEqual(bridge.status()['state'],'disconnected')
    def test_disk_selected_by_ancestry_and_fat_only(self):
        tree={'IORegistryEntryChildren':[{'IOObjectClass':'IOMedia','BSD Name':'disk99s1'}]}
        info={'MountPoint':'/Volumes/test-pico','FilesystemType':'msdos','ParentWholeDisk':'disk99'}
        with patch.object(bridge,'usb_nodes',return_value=[tree]),patch.object(bridge,'run',return_value=plistlib.dumps(info)):
            self.assertEqual(bridge.sd_volume(),(Path('/Volumes/test-pico'),'/dev/disk99'))
        with patch.object(bridge,'usb_nodes',return_value=[]):self.assertIsNone(bridge.sd_volume())
    def test_disappearing_disk_during_sd_mode_is_pending_not_fatal(self):
        tree={'IOObjectClass':'IOMedia','BSD Name':'disk99s1'}
        with patch.object(bridge,'usb_nodes',return_value=[tree]),patch.object(bridge,'run',side_effect=RuntimeError('Could not find disk: /dev/disk99s1')):
            self.assertIsNone(bridge.sd_volume())
        with patch.object(bridge,'usb_nodes',return_value=[tree]),patch.object(bridge,'run',side_effect=RuntimeError('permission denied')):
            with self.assertRaisesRegex(RuntimeError,'permission denied'):bridge.sd_volume()
    def test_write_backup_verify_and_eject_order(self):
        c=model.from_xml((ROOT/'release/config.xml').read_bytes())
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);mount=root/'volume';mount.mkdir();backup=root/'backup';backup.mkdir()
            old=b'old data';(mount/'config.xml').write_bytes(old)
            events=[]
            with patch.object(bridge,'sd_volume',return_value=(mount,'/dev/disk99')),patch.object(bridge,'backup_dir',return_value=backup),patch.object(bridge,'run',side_effect=lambda args,timeout=15:events.append(args) or b''),patch.object(bridge,'command',return_value='HP OK REBOOT'),patch.object(bridge,'status',return_value={'state':'ready','info':{'disk_mode':False,'keyboard_ready':True,'mouse_ready':True}}):
                result=bridge.sd_operation(c,events.append)
            self.assertEqual((backup/'config-before.xml').read_bytes(),old)
            self.assertEqual(model.from_xml((mount/'config.xml').read_bytes()),c)
            self.assertEqual((mount/'last.txt').read_bytes(),b'0')
            self.assertIn(['diskutil','eject','/dev/disk99'],events)
            self.assertEqual(result['config'],c)
    def test_disconnect_never_writes(self):
        c=model.from_xml((ROOT/'release/config.xml').read_bytes())
        with patch.object(bridge,'sd_volume',return_value=None),patch.object(bridge,'command',side_effect=RuntimeError('device missing')),patch.object(bridge,'backup_dir') as backup:
            with self.assertRaisesRegex(RuntimeError,'device missing'): bridge.sd_operation(c,lambda _:None)
            backup.assert_not_called()

class HttpTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.http=server.ThreadingHTTPServer(('127.0.0.1',0),server.Handler)
        threading.Thread(target=cls.http.serve_forever,daemon=True).start()
        cls.port=cls.http.server_port
    @classmethod
    def tearDownClass(cls):cls.http.shutdown();cls.http.server_close()
    def request(self,path,body=None,headers=None):
        conn=http.client.HTTPConnection('127.0.0.1',self.port,timeout=5)
        conn.request('POST' if body is not None else 'GET',path,json.dumps(body) if body is not None else None,headers or {})
        r=conn.getresponse();data=r.read();conn.close();return r.status,data
    def test_origin_and_token_required(self):
        with patch.object(server,'launch') as launch:
            code,_=self.request('/api/flash',{}, {'Origin':'https://other-site.example','Content-Type':'application/json'})
            self.assertEqual(code,403);launch.assert_not_called()
    def test_import_export_preserves_all_actions(self):
        code,data=self.request('/api/bootstrap');self.assertEqual(code,200);boot=json.loads(data)
        h={'Content-Type':'application/json','Origin':f'http://127.0.0.1:{self.port}','X-Hapticpad-Token':boot['token']}
        code,data=self.request('/api/export',{'config':boot['config']},h);self.assertEqual(code,200)
        code,data=self.request('/api/import',{'xml':json.loads(data)['xml']},h)
        self.assertEqual(code,200);self.assertEqual(json.loads(data)['config'],boot['config'])
    def test_no_filesystem_traversal(self):self.assertEqual(self.request('/../bridge.py')[0],404)
    def test_sdr_append_endpoint_and_download(self):
        code,data=self.request('/api/bootstrap');boot=json.loads(data)
        h={'Content-Type':'application/json','Origin':f'http://127.0.0.1:{self.port}','X-Hapticpad-Token':boot['token']}
        with patch.object(server,'launch') as launch:
            code,data=self.request('/api/sdr-config',{'config':boot['config']},h)
            self.assertEqual(code,200);self.assertEqual(len(json.loads(data)['config']['profiles']),len(boot['config']['profiles'])+(0 if any(p['name']=='SDR通用' for p in boot['config']['profiles']) else 1))
            launch.assert_not_called()
        code,data=self.request('/sdr-console.zip');self.assertEqual(code,200);self.assertTrue(data.startswith(b'PK'))
    def test_hardware_jobs_are_serialized(self):
        server.LOCK.acquire()
        try:
            with self.assertRaises(ValueError):server.launch('flash',{})
        finally:server.LOCK.release()

if __name__=='__main__':unittest.main(verbosity=2)
