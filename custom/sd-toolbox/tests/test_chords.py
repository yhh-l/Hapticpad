import copy
from pathlib import Path
import unittest
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import model
ROOT=Path(__file__).resolve().parents[1]
class ChordConfigTests(unittest.TestCase):
    def setUp(self):self.config=model.from_xml((ROOT/'release/config.xml').read_bytes())
    def test_legacy_defaults_preserve_navigation_and_six_bindings(self):
        for p in self.config['profiles']:
            self.assertEqual(p['aux_mode'],'legacy');self.assertEqual(len(p['buttons']),6)
            self.assertEqual(len(p['extra_buttons']),2);self.assertEqual(p['chords'],[])
        self.assertEqual(model.from_xml(model.to_xml(self.config)),self.config)
    def test_all_255_masks_fit_single_profile_and_roundtrip(self):
        self.config['profiles']=self.config['profiles'][:1];p=self.config['profiles'][0];p['aux_mode']='all_custom'
        p['extra_buttons'][0]={'label':'保存','actions':[[0,17],[0,83],[0,0]]}
        p['extra_buttons'][1]={'label':'删除','actions':[[0,46],[0,0],[0,0]]}
        p['chords']=[dict(mask=m,label='组合'+str(m),actions=[[0,17],[10,65],[0,0]]) for m in range(1,256) if m&(m-1)]
        self.assertEqual(len(p['chords']),247)
        data=model.to_xml(self.config);self.assertLessEqual(len(data),65536)
        self.assertEqual(model.from_xml(data),self.config)
    def test_invalid_duplicate_masks_and_auxiliary_settings(self):
        for value in [0,1,2,128,256,-1,True]:
            c=copy.deepcopy(self.config);c['profiles'][0]['chords']=[dict(mask=value,label='测试',actions=[[0,0]]*3)]
            with self.assertRaises(ValueError):model.to_xml(c)
        for field,value in [('aux_mode','invalid'),('chord_window_ms',49),('chord_window_ms',251),('extra_buttons',[])]:
            c=copy.deepcopy(self.config);c['profiles'][0][field]=value
            with self.assertRaises(ValueError):model.to_xml(c)
        p=self.config['profiles'][0];p['chords']=[dict(mask=3,label='复制',actions=[[0,17],[0,67],[0,0]])]*2
        with self.assertRaisesRegex(ValueError,'重复'):model.to_xml(self.config)
    def test_preserved_inactive_small_key_chords_and_custom_menu_pair(self):
        p=self.config['profiles'][0];p['chords']=[dict(mask=192,label='保存',actions=[[0,17],[0,83],[0,0]])]
        for mode in ['legacy','custom','all_custom']:
            p['aux_mode']=mode;self.assertEqual(model.from_xml(model.to_xml(self.config)),self.config)
    def test_total_file_size_limit_and_new_sdr_has_no_inherited_chords(self):
        p=self.config['profiles'][0];p['chords']=[dict(mask=3,label='保存',actions=[[0,17],[0,83],[0,0]])]
        p['aux_mode']='custom'
        sdr=model.add_sdr_profile(self.config)['profiles'][-1]
        self.assertEqual(sdr['chords'],[]);self.assertEqual(sdr['aux_mode'],'legacy')
        p['chords']=[dict(mask=m,label='组合'+str(m),actions=[[0,17],[10,65],[0,0]]) for m in range(1,256) if m&(m-1)]
        self.config['profiles']=[copy.deepcopy(p) for _ in range(256)]
        with self.assertRaisesRegex(ValueError,'8MB'):model.to_xml(self.config)
if __name__=='__main__':unittest.main()
