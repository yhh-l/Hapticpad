"""Compile production Arduino String parser and signed coordinate helper on host.
Usage: python3 tests/check_firmware.py /path/to/ArduinoCore-API/api
"""
from pathlib import Path
import subprocess
import os
import sys
import tempfile
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import model
root=Path(__file__).resolve().parents[1]; api=Path(sys.argv[1]).resolve()
parser=(root/'firmware/MacroPad/config.ino').read_text().split('void setLEDProfile()')[0]
# The Arduino sketch preprocessor supplies this forward declaration on-device.
parser=parser.replace('const String &fallback = ""','const String &fallback')
header=r'''
#include <string>
#include <fstream>
#include <iterator>
#include <iostream>
#include <cstring>
#include <cstdint>
#include <cassert>
#include <cmath>
#include "String.h"
#include "Common.h"
#include "device.h"
using namespace arduino;
String textTag(const String &, const char *, const String &fallback = "");
template<class T> T min(T a, T b) { return a < b ? a : b; }
'''
menu_text=(root/'firmware/MacroPad/menu.ino').read_text()
parser+=menu_text[menu_text.index('void replaceSetting('):menu_text.index('bool persistSettingsXML(')]
main=r'''
int main() {
  std::ifstream input(std::getenv("HAPTIC_CONFIG")); std::string xml((std::istreambuf_iterator<char>(input)), {});
  configXML = xml.c_str();
  if (std::getenv("HAPTIC_CHORDS")) {
    Profile out; assert(readProfile(0,out));
    assert(out.auxiliaryMode==2 && out.chordWindow==150 && out.chordCount==247);
    assert(out.actions[6][0]==17 && out.actions[6][1]==83 && out.actions[7][0]==46);
    bool seen[256]={};
    for(unsigned i=0;i<out.chordCount;i++){
      auto &binding=out.chords[i];assert(binding.mask && (binding.mask&(binding.mask-1)) && !seen[binding.mask]);seen[binding.mask]=true;
      assert(binding.actions[0]==17 && binding.actions[1]==65 && binding.delays[1]==10);
    }
    for(unsigned mask=1;mask<256;mask++)assert(seen[mask]==bool(mask&(mask-1)));
    String block=profileBlock(0);replaceSetting(block,"HapticStrength","90");replaceSetting(block,"MissingSetting","42");
    assert(textTag(block,"HapticStrength")=="90" && textTag(block,"MissingSetting")=="42");
    configXML.replace("<Chord mask=\"3\">","<Chord mask=\"1\">");assert(!readProfile(0,out));
    std::cout<<"Production parser: 8 singles, all 247 chords, auxiliary/window fields, invalid masks and menu XML edits OK\n";
    return 0;
  }
  if (std::getenv("HAPTIC_FLYWHEEL")) {
    Profile out;assert(readProfile(0,out));assert(out.mode==3 && out.feedback.mode==FOLLOW);
    assert(out.flywheel.durationMs==27000 && out.flywheel.halfLifeMs==9000);
    assert(out.flywheel.triggerRPM==7 && out.flywheel.maxRPM==360 && out.flywheel.boostPercent==130);
    assert(out.flywheel.brakePercent==65 && out.flywheel.brakeMs==100 && out.flywheel.captureMs==60);
    assert(fabsf(out.flywheel.voltage-.7f)<.0001f);
    assert(out.actions[0][0]==91 && out.actions[0][1]==67);
    configXML.replace("<FlywheelTriggerRPM>7</FlywheelTriggerRPM>","<FlywheelTriggerRPM>40</FlywheelTriggerRPM>");
    configXML.replace("<FlywheelMaxRPM>360</FlywheelMaxRPM>","<FlywheelMaxRPM>30</FlywheelMaxRPM>");
    assert(!readProfile(0,out));
    std::cout<<"Production parser: Flywheel settings, shortcut preservation, invalid trigger rejection OK\n";return 0;
  }
  if (std::getenv("HAPTIC_SDR")) {
    Profile out; assert(readProfile(16,out));
    assert(!strcmp(out.name,"SDR通用")); assert(out.wheelKey==18);
    const char *names[] = {"步进减","步进加","模式前","模式后","频段前","频段后"};
    for(int i=0;i<6;++i) {
      assert(!strcmp(out.labels[i],names[i]));
      assert(out.actions[i][0]==17 && out.actions[i][1]==18 && out.actions[i][2]==112+i);
    }
    assert(!readProfile(17,out));
    std::cout << "Production parser: SDR profile 17, six bindings and Alt-wheel OK\n";
    return 0;
  }
  const char *labels[] = {"复制","粘贴","剪切","全选","撤销","退格"};
  int keys[] = {67,86,88,65,90};
  for (int p=0;p<2;++p) {
    Profile out; assert(readProfile(p,out));
    std::cout << out.name << '\n';
    if(p==0){assert(out.feedback.mode==FOLLOW);assert(out.feedback.strength==25);assert(fabsf(out.feedback.voltage-.8f)<1e-5f);}
    else {assert(out.feedback.mode==ASSIST);assert(out.feedback.assist==40);assert(out.feedback.detents==64);assert(out.feedback.triggerRPM==6);assert(out.feedback.assistMs==300);assert(fabsf(out.feedback.voltage-.9f)<1e-5f);assert(out.ledMode==7);assert(out.ledDirection==-1);assert(out.ledOffset==19);}
    for (int b=0;b<6;++b) {
      assert(!strcmp(out.labels[b],labels[b]));
      assert(out.actions[b][0] == (b<5?(p==0?91:17):8));
      assert(out.actions[b][1] == (b<5?keys[b]:0)); assert(out.actions[b][2]==0);
      for (int a=0;a<3;++a) assert(out.delays[b][a] == (p==1&&b==2&&a==1?123:0));
    }
  }
  Profile invalid; assert(!readProfile(2,invalid));
  for (int sign : {-1,1}) {
    for (float angle : {-12.0f, -0.1f, 0.0f, 0.1f, 12.0f}) {
      const float offset=0.3f, step=6.283185307f/40;
      float current=shaftCoordinate(angle,sign,offset);
      float target=shaftCoordinate(angle+step,sign,offset);
      assert(fabsf((target-current)*sign-step)<0.00001f);
      assert(fabsf(shaftCoordinate(angle,sign,offset)-current)<0.00001f);
    }
  }
  configXML.replace("<Action>0,91</Action>","<Broken>0,91</Broken>");
  assert(!readProfile(0,invalid));
  std::cout << "Production parser: all 12 labels, 36 actions/delays and both motor directions OK\n";
}
'''
with tempfile.TemporaryDirectory(prefix='haptic-cn-parser-') as tmp:
    temp=Path(tmp); config=model.from_xml((root/'release/config.xml').read_bytes());config['profiles'][1]['buttons'][2]['actions'][1][0]=123;config['profiles'][1]['feedback']={**model.FEEDBACK_PRESETS['assist_medium']['feedback'],'detents':64,'trigger_rpm':6}
    config['profiles'][1].update(led='KnobTrail',led_direction=-1,led_offset=19)
    (temp/'config.xml').write_bytes(model.to_xml(config));(temp/'check.cpp').write_text(header+parser+main)
    subprocess.run(['clang++','-std=c++17','-iquote',str(api),'-I',str(api.parent),'-I',str(root/'firmware/MacroPad'),str(temp/'check.cpp'),str(api/'String.cpp'),str(api.parent/'test/src/itoa.cpp'),str(api.parent/'test/src/dtostrf.cpp'),'-o',str(temp/'check')],check=True)
    subprocess.run([str(temp/'check')], env=dict(os.environ,HAPTIC_CONFIG=str(temp/'config.xml')),check=True)

    sdr=model.configuration_templates(model.from_xml((root/'release/config.xml').read_bytes()))['haptics']['config']
    (temp/'sdr.xml').write_bytes(model.to_xml(model.add_sdr_profile(sdr)))
    subprocess.run([str(temp/'check')],env=dict(os.environ,HAPTIC_CONFIG=str(temp/'sdr.xml'),HAPTIC_SDR='1'),check=True)

    full=model.from_xml((root/'release/config.xml').read_bytes());full['profiles']=full['profiles'][:1]
    full['profiles'][0].update(aux_mode='all_custom',chord_window_ms=150,
        extra_buttons=[dict(label='保存',actions=[[0,17],[0,83],[0,0]]),dict(label='删除',actions=[[0,46],[0,0],[0,0]])],
        chords=[dict(mask=m,label='组合'+str(m),actions=[[0,17],[10,65],[0,0]]) for m in range(1,256) if m&(m-1)])
    (temp/'full.xml').write_bytes(model.to_xml(full))
    subprocess.run([str(temp/'check')],env=dict(os.environ,HAPTIC_CONFIG=str(temp/'full.xml'),HAPTIC_CHORDS='1'),check=True)

    fly=model.from_xml((root/'release/config.xml').read_bytes())
    fly['profiles'][0]['mode']='Flywheel'
    fly['profiles'][0]['flywheel'].update(duration_ms=27000,half_life_ms=9000,trigger_rpm=7,max_rpm=360,boost_percent=130,brake_percent=65,brake_ms=100,capture_ms=60,voltage=.7)
    (temp/'fly.xml').write_bytes(model.to_xml(fly))
    subprocess.run([str(temp/'check')],env=dict(os.environ,HAPTIC_CONFIG=str(temp/'fly.xml'),HAPTIC_FLYWHEEL='1'),check=True)
