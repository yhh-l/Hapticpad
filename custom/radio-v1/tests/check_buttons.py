"""Exercise the production debounce/chord/menu dispatcher with fake GPIO/HID."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'MacroPad/MacroPad.ino').read_text()
macro=source[source.index('void beginMacro('):source.index('bool commitProfile(')]
buttons=source[source.index('bool commitProfile('):source.index('void printJSONString(')]
menu=(root/'MacroPad/menu.ino').read_text()
entries=menu[:menu.index('void confirmMenuField(')]
header=r'''
#include <cassert>
#include <cstring>
#include <string>
#include <vector>
#include <iostream>
#include "device.h"
#include "number_macro.h"
NumberMacro numberMacro;
using String=std::string;
uint32_t now=0,pins=0,buttonsMask=0;
uint32_t millis(){return now;}
int constrain(int n,int lo,int hi){return n<lo?lo:n>hi?hi:n;}
template<class T>T max(T a,T b){return a>b?a:b;}
const uint8_t buttonPins[]={9,1,0,12,11,10,13,14};
const int LOW=0;
int digitalRead(int pin){for(int i=0;i<8;i++)if(buttonPins[i]==pin)return (pins&(1<<i))?0:1;return 1;}
void copyText(char *out,const String &value){strncpy(out,value.c_str(),31);out[31]=0;}
Profile profile;
std::atomic<bool> menuOpen{false};std::atomic<unsigned> generation{0};std::atomic<int> wheelEvents{0};
IdleTimer idleTimer;
std::atomic<int32_t> rawTelemetry{0};
void setDeviceStandby(bool){}
SettingsMenu settingsMenu;
uint8_t menuKind=0;
int activeProfile=0,menuSelection=0,profileCount=17,saves=0,enters=0;
bool sdDetected=true,macroActive=false,macroRelease=false;
int macroStage=0;uint8_t macroActions[3]={},macroModifier=0,lastBindingMask=0;
uint16_t macroDelays[3]={};uint32_t macroDue=0,lastBindingAt=0;char lastBindingLabel[32]={};
ChordRecognizer chordRecognizer;
struct FakeKeyboard{std::vector<int> keys,modifiers;int releases=0;bool ready(){return true;}void keyboardRelease(int){++releases;}void keyboardReport(int,int modifier,const uint8_t *key){if(key[0]){keys.push_back(key[0]);modifiers.push_back(modifier);}}}usb_keyboard;
void releaseKeys(){usb_keyboard.keyboardRelease(0);numberMacro.cancel();macroActive=false;macroRelease=false;macroModifier=0;}
int checkModifiers(int code){return code==17?1:code==16?2:code==18?4:code==91?8:0;}
int convertKeycode(int code){return code;}
std::atomic<bool> configBusy{false},configAcknowledged{false};
bool parseOK=true,ackOK=true;
void delay(int ms){now+=ms;if(ackOK)configAcknowledged=true;}
bool readProfile(int index,Profile &out){out=profile;out.name[0]='A'+index;return parseOK;}
void setLEDProfile(){}
struct FakeHUD{void clear(){}}radioHUD;
constexpr int O_WRONLY=1,O_CREAT=2,O_TRUNC=4;
struct FsFile{explicit operator bool()const{return true;}void print(int){}void close(){}};
struct FakeSD{FsFile open(const char*,int){return {};}}SD;
void confirmMenuField(){settingsMenu.confirm();}
bool saveMenuSettings(){++saves;return true;}
'''
main=r'''
void drive(uint8_t mask,int duration){pins=mask;for(int i=0;i<duration;i++){++now;readButtons();tickMacro();}}
void prepare(int mode){drive(0,200);profile=Profile{};profile.auxiliaryMode=mode;menuOpen=false;chordRecognizer.cancel();usb_keyboard.keys.clear();activeProfile=saves=enters=0;for(int i=0;i<8;i++)profile.actions[i][0]=65+i;profile.chordCount=3;profile.chords[0].mask=3;profile.chords[0].actions[0]=112;profile.chords[1].mask=7;profile.chords[1].actions[0]=113;profile.chords[2].mask=192;profile.chords[2].actions[0]=114;}
void expect(std::vector<int> keys){assert(usb_keyboard.keys==keys);usb_keyboard.keys.clear();}
int main(){
 prepare(0);drive(1,40);drive(0,80);expect({65});drive(2,40);drive(0,80);expect({66});drive(3,180);drive(0,120);expect({112});
 drive(1,30);drive(3,30);drive(7,180);drive(0,120);expect({113});
 drive(9,180);drive(1,50);drive(0,120);expect({});
 drive(1,400);drive(0,120);expect({65});
 drive(64,100);drive(0,120);assert(activeProfile==1&&!menuOpen);expect({});
 drive(128,100);drive(0,120);assert(activeProfile==0&&!menuOpen);expect({});
 drive(64,800);assert(menuOpen&&menuKind==1);drive(0,120);assert(menuOpen);
 drive(2,100);drive(0,100);assert(menuSelection==1);drive(128,100);drive(0,100);assert(!menuOpen&&activeProfile==1);expect({});
 drive(128,800);assert(menuOpen&&menuKind==2);drive(0,120);assert(menuOpen);
 drive(1,100);drive(0,100);assert(menuOpen&&settingsMenu.item==0&&settingsMenu.editing);expect({});
 drive(128,100);drive(0,100);assert(menuOpen&&!settingsMenu.editing);
 drive(64,800);assert(!menuOpen);drive(0,100);assert(activeProfile==1);expect({});
 drive(128,800);drive(0,100);assert(menuOpen&&menuKind==2);drive(128,800);assert(!menuOpen&&saves==1);drive(0,100);assert(activeProfile==1);
 // Exact reported flow, including release after long7 and a held confirm key.
 prepare(0);drive(64,800);assert(menuOpen&&menuKind==1);drive(0,80);
 turnMenu(4);assert(menuSelection==4);drive(128,100);assert(menuOpen&&activeProfile==0);
 drive(0,100);assert(!menuOpen&&activeProfile==4&&profile.name[0]=='E');expect({});
 // Parse failure preserves running profile and selection; short8 retries.
 drive(64,800);drive(0,100);turnMenu(2);parseOK=false;
 drive(128,100);drive(0,100);assert(menuOpen&&settingsMenu.error&&activeProfile==4&&profile.name[0]=='E');
 parseOK=true;drive(128,100);drive(0,100);assert(!menuOpen&&activeProfile==6&&!settingsMenu.error);
 // Motor handshake timeout also keeps chooser open; successful retry exits.
 drive(64,800);drive(0,100);turnMenu(1);ackOK=false;
 drive(128,100);drive(0,100);assert(menuOpen&&settingsMenu.error&&activeProfile==6&&!configBusy);
 ackOK=true;drive(128,100);drive(0,100);assert(!menuOpen&&activeProfile==7);
 // Entry release and cancellation cannot advance or apply a profile.
 drive(64,800);drive(0,100);turnMenu(-3);drive(64,100);drive(0,100);assert(!menuOpen&&activeProfile==7);expect({});
 // Mac shortcut reports must resume after cancelling or confirming the chooser.
 prepare(0);profile.actions[0][0]=91;profile.actions[0][1]=67;profile.actions[0][2]=0;
 drive(64,800);drive(0,100);drive(64,100);drive(0,100);assert(!menuOpen);
 usb_keyboard.modifiers.clear();drive(1,60);drive(0,120);expect({67});assert(usb_keyboard.modifiers.back()==8);
 drive(64,800);drive(0,100);turnMenu(1);drive(128,100);drive(0,100);assert(!menuOpen);
 usb_keyboard.modifiers.clear();drive(1,60);drive(0,120);expect({67});assert(usb_keyboard.modifiers.back()==8);
 prepare(1);drive(64,180);drive(0,100);expect({71});
 drive(192,200);assert(!menuOpen);drive(0,120);expect({114});
 drive(192,1000);assert(menuOpen);expect({});drive(0,100);drive(64,800);drive(0,100);assert(!menuOpen);
 prepare(2);drive(192,1000);assert(!menuOpen);drive(0,100);expect({114});
 prepare(0);profile.chordCount=4;auto &text=profile.chords[3];text.mask=56;strcpy(text.numberText,"813816");text.enterAfterText=true;
 usb_keyboard.modifiers.clear();int released=usb_keyboard.releases;
 drive(56,1000);drive(0,150);expect({56,49,51,56,49,54,13});assert(usb_keyboard.releases-released==7);
 for(int m:usb_keyboard.modifiers)assert(m==0);
 drive(56,40);drive(0,500);expect({56,49,51,56,49,54,13});
 drive(56,140);releaseKeys();drive(0,500);usb_keyboard.keys.clear();assert(!numberMacro.active()&&!macroActive);
 std::cout<<"PASS: production GPIO debounce, double/triple dispatch, no subset/held repeats, 7/8 navigation, settings isolation and long exit/save, custom pair versus all-custom\n";
}
'''
with tempfile.TemporaryDirectory(prefix='haptic-buttons-') as temp:
    folder=Path(temp);(folder/'check.cpp').write_text(header+macro+entries+buttons+main)
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-I',str(root/'MacroPad'),str(folder/'check.cpp'),'-o',str(folder/'check')],check=True)
    subprocess.run([str(folder/'check')],check=True)
