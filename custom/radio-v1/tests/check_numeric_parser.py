"""Compile and test the production numeric XML parser using Arduino String."""
from pathlib import Path
import subprocess,sys,tempfile
root=Path(__file__).resolve().parents[1];api=Path(sys.argv[1]).resolve()
parser=(root/'MacroPad/config.ino').read_text().split('void setLEDProfile()')[0].replace('const String &fallback = ""','const String &fallback')
header=r'''
#include <string>
#include <cstring>
#include <cstdint>
#include <cassert>
#include <cmath>
#include "String.h"
#include "Common.h"
#include "device.h"
using namespace arduino;
String textTag(const String &, const char *, const String &fallback = "");
template<class T>T min(T a,T b){return a<b?a:b;}
'''
buttons='<MacroButton><Action>0,0</Action><Action>0,0</Action><Action>0,0</Action><Label>key</Label></MacroButton>'*6
xml='<Configuration><Profiles><Profile name="Win"><MacroButtons>'+buttons+'</MacroButtons><Chords><Chord mask="56"><Action>0,0</Action><Action>0,0</Action><Action>0,0</Action><Label>number</Label><NumberText>813816</NumberText><EnterAfterText>1</EnterAfterText></Chord></Chords></Profile></Profiles></Configuration>'
main='int main(){configXML=R"xml('+xml+')xml";'+r'''
 Profile p;assert(readProfile(0,p));assert(p.chordCount==1&&p.chords[0].mask==56);
 assert(!strcmp(p.chords[0].numberText,"813816")&&p.chords[0].enterAfterText);
 configXML.replace("Win","Mac");assert(readProfile(0,p));assert(!strcmp(p.chords[0].numberText,"813816"));
 String saved=configXML;
 configXML.replace("813816","813x16");assert(!readProfile(0,p));configXML=saved;
 configXML.replace("813816","1234567890123456");assert(!readProfile(0,p));configXML=saved;
 configXML.replace("<EnterAfterText>1</EnterAfterText>","<EnterAfterText>2</EnterAfterText>");assert(!readProfile(0,p));configXML=saved;
 configXML.replace("<NumberText>813816</NumberText>","");assert(readProfile(0,p));assert(!p.chords[0].numberText[0]);
}
'''
with tempfile.TemporaryDirectory() as tmp:
 folder=Path(tmp);(folder/'test.cpp').write_text(header+parser+main)
 subprocess.run(['g++','-std=c++17','-iquote',str(api),'-I',str(api.parent),'-I',str(root/'MacroPad'),str(folder/'test.cpp'),str(api/'String.cpp'),str(api.parent/'test/src/itoa.cpp'),str(api.parent/'test/src/dtostrf.cpp'),'-o',str(folder/'test')],check=True)
 subprocess.run([str(folder/'test')],check=True)
print('PASS: production XML NumberText/EnterAfterText on Win and Mac, malformed input rejected')
