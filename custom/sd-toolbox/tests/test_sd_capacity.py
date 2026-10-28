import copy,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import model
class SDCapacityTests(unittest.TestCase):
    def setUp(self):self.c=model.from_xml((Path(__file__).resolve().parents[1]/'release/config.xml').read_bytes())
    def test_256_profiles_over_old_limit_roundtrip(self):
        p=self.c['profiles'][0]
        p['chords']=[dict(mask=56,label='数字并回车',actions=[[0,0]]*3,number_text='813816',enter_after_text=True)]
        self.c['profiles']=[dict(copy.deepcopy(p),name='Mac'+str(i)) for i in range(256)]
        data=model.to_xml(self.c);self.assertGreater(len(data),65536)
        self.assertEqual(model.from_xml(data),self.c)
        self.c['profiles'].append(copy.deepcopy(p))
        with self.assertRaisesRegex(ValueError,'256'):model.to_xml(self.c)
    def test_raw_profile_bounds_cannot_be_bypassed_by_padding(self):
        data=model.to_xml(self.c)
        for pad in (b' '*65536,('中'*22000).encode()):
            bad=data.replace(b'</Profile>',pad+b'</Profile>',1)
            with self.assertRaisesRegex(ValueError,'单套'):model.from_xml(bad)
        bad=data.replace(b'</Settings>',b' '*4096+b'</Settings>',1)
        with self.assertRaisesRegex(ValueError,'4KB'):model.from_xml(bad)
    def test_total_and_structure_bounds(self):
        with self.assertRaisesRegex(ValueError,'8MB'):model.from_xml(b' '*(8*1024*1024+1))
        for xml in (b'<!DOCTYPE Configuration><Configuration/>',b'<Configuration><Profiles/><Profiles/></Configuration>',b'<Configuration><Other/></Configuration>'):
            with self.assertRaises(ValueError):model.from_xml(xml)
    def test_numeric_macro_roundtrip_and_invalid_values(self):
        p=self.c['profiles'][0]
        for enter in (False,True):
            p['chords']=[dict(mask=56,label='数字宏',actions=[[0,0]]*3,number_text='813816',enter_after_text=enter)]
            self.assertEqual(model.from_xml(model.to_xml(self.c)),self.c)
        for n in ('123x','１２３','1'*16):
            p['chords'][0]['number_text']=n
            with self.assertRaises(ValueError):model.to_xml(self.c)
if __name__=='__main__':unittest.main()
