#include <cassert>
#include <iostream>
#include "../firmware/MacroPad/settings_menu.h"
int main(){
 SettingsMenu menu;MenuValues values;values.profile=2;values.strength=25;menu.begin(values);
 menu.turn(50,17);assert(menu.item==5&&!menu.editing);menu.turn(-50,17);assert(menu.item==0);
 menu.select(1);menu.turn(3,17);assert(menu.values.strength==40);menu.back();assert(menu.values.strength==25&&!menu.editing);
 menu.select(1);menu.turn(100,17);assert(menu.values.strength==100);menu.confirm();assert(!menu.editing&&menu.values.strength==100);
 menu.select(2);menu.turn(-10,17);assert(menu.values.damping==0);menu.confirm();
 menu.select(0);menu.turn(99,17);assert(menu.values.profile==16);menu.back();assert(menu.values.profile==2);
 menu.select(3);menu.turn(-100,17);assert(menu.values.led==5);menu.turn(100,17);assert(menu.values.led==3);menu.back();assert(menu.values.led==7);
 menu.select(4);menu.turn(100,17);assert(menu.values.brightness==255);menu.turn(-100,17);assert(menu.values.brightness==0);
 menu.select(5);menu.turn(100,17);assert(menu.values.wheel==3);menu.back();assert(menu.values.wheel==0);
 menu.error=true;menu.begin(values);assert(!menu.error&&!menu.editing&&menu.values.strength==25);
 std::cout<<"PASS: settings selection, knob changes, field confirm/cancel, bounds, reset and profile selection\n";
}
