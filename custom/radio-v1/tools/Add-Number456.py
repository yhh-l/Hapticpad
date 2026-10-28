"""Add the same standalone 4+5+6 numeric macro to all Mac/Win profiles."""
import argparse,json
from pathlib import Path
import xml.etree.ElementTree as ET

def build(data):
    root=ET.fromstring(data);profiles=root.find('Profiles')
    if profiles is None:raise ValueError('Missing Profiles')
    changed=[]
    for p in profiles:
        name=p.get('name','')
        if not name.startswith(('Win','Mac')):continue
        chords=p.find('Chords')
        if chords is None:chords=ET.SubElement(p,'Chords')
        for old in list(chords):
            if old.get('mask')=='56':chords.remove(old)
        node=ET.SubElement(chords,'Chord',mask='56')
        for _ in range(3):ET.SubElement(node,'Action').text='0,0'
        ET.SubElement(node,'Label').text='数字并回车'
        ET.SubElement(node,'NumberText').text='813816'
        ET.SubElement(node,'EnterAfterText').text='1'
        if p.find('ChordWindow') is None:ET.SubElement(p,'ChordWindow').text='60'
        changed.append(name)
    if not changed:raise ValueError('No Windows/Mac profiles found')
    ET.indent(root,space='  ')
    out=ET.tostring(root,encoding='utf-8',xml_declaration=True)
    if len(out)>65536:raise ValueError('Configuration exceeds 64 KB; no write allowed')
    return out,changed
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('destination',type=Path);a=p.parse_args()
    data,names=build(a.source.read_bytes());a.destination.write_bytes(data)
    print(json.dumps({'bytes':len(data),'updated':names,'windows':sum(x.startswith('Win') for x in names),'mac':sum(x.startswith('Mac') for x in names)},ensure_ascii=False))
