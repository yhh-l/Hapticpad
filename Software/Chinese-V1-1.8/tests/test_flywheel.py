import copy
import json
import sys
from pathlib import Path
from unittest.mock import patch
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import model
import server
import test_toolbox
ROOT=Path(__file__).resolve().parents[1]

class FlywheelConfigTests(unittest.TestCase):
    def setUp(self): self.c=model.from_xml((ROOT/'release/config.xml').read_bytes())
    def test_legacy_defaults_and_unchanged_buttons(self):
        self.assertEqual(self.c['profiles'][0]['flywheel'],model.FLYWHEEL_DEFAULTS)
        buttons=copy.deepcopy(self.c['profiles'][0]['buttons'])
        self.c['profiles'][0]['mode']='Flywheel'
        for preset in model.FLYWHEEL_PRESETS.values():
            self.c['profiles'][0]['flywheel']=preset['settings'].copy()
            updated=model.from_xml(model.to_xml(self.c))
            self.assertEqual(updated,self.c)
            self.assertEqual(updated['profiles'][0]['buttons'],buttons)
    def test_reject_invalid_limits_and_bad_floats(self):
        for key,(lo,hi) in model.FLYWHEEL_RANGES.items():
            for value in (lo-1,hi+1,True,1.5,float('nan')):
                c=copy.deepcopy(self.c);c['profiles'][0]['flywheel'][key]=value
                with self.assertRaises(ValueError,msg=f'{key}={value}'): model.to_xml(c)
        for value in (-.1,1,True,float('nan'),float('inf')):
            c=copy.deepcopy(self.c);c['profiles'][0]['flywheel']['voltage']=value
            with self.assertRaises(ValueError):model.to_xml(c)
        self.c['profiles'][0]['flywheel'].update(trigger_rpm=40,max_rpm=30)
        with self.assertRaisesRegex(ValueError,'无极触发'):model.to_xml(self.c)
    def test_requires_follow_and_zero_voltage_is_allowed(self):
        p=self.c['profiles'][0];p['mode']='Flywheel';p['flywheel']['voltage']=0
        self.assertEqual(model.from_xml(model.to_xml(self.c)),self.c)
        p['feedback']['mode']='Damping'
        with self.assertRaises(ValueError):model.to_xml(self.c)
    def test_original_sd_profiles_preserved_in_preview(self):
        original=model.from_xml((ROOT/'release/original-sd.xml').read_bytes())
        candidate=model.from_xml((ROOT/'release/preview-config.xml').read_bytes())
        self.assertEqual(candidate['profiles'][:len(original['profiles'])],original['profiles'])
        self.assertEqual(len(candidate['profiles']),len(original['profiles'])+2)
        self.assertEqual([p['mode'] for p in candidate['profiles'][-2:]],['Flywheel']*2)
        self.assertFalse(candidate['motor_enabled'])
        for p in candidate['profiles'][-2:]:
            source=next(x for x in original['profiles'] if x['name'].startswith('Mac' if p['name'].startswith('Mac') else 'Win'))
            self.assertEqual(p['buttons'],source['buttons'])

class PreviewHttpTests(unittest.TestCase):
    setUpClass=classmethod(test_toolbox.HttpTests.setUpClass.__func__)
    tearDownClass=classmethod(test_toolbox.HttpTests.tearDownClass.__func__)
    request=test_toolbox.HttpTests.request
    def test_preview_blocks_all_device_mutations_before_launch(self):
        self.addCleanup(setattr,server,"PREVIEW_ONLY",server.PREVIEW_ONLY)
        server.PREVIEW_ONLY=True
        code,data=self.request('/api/bootstrap');boot=json.loads(data)
        self.assertTrue(boot['preview_only'])
        self.assertEqual(boot['flywheel_defaults'],model.FLYWHEEL_DEFAULTS)
        h={'Content-Type':'application/json','Origin':f'http://127.0.0.1:{self.port}','X-Hapticpad-Token':boot['token']}
        for action in ('flash','read','write','motor','resume'):
            with patch.object(server,'launch') as launch:
                code,data=self.request('/api/'+action,{},h)
                self.assertEqual(code,400);self.assertIn('本地无极候选',json.loads(data)['error'])
                launch.assert_not_called()
            with self.assertRaisesRegex(ValueError,'本地无极候选'):server.launch(action,{})
