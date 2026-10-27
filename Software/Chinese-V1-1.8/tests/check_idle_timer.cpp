#include <cassert>
#include <limits>
#include "idle_timer.h"
int main(){
 IdleTimer idle;
 assert(!idle.update(10,1, false));assert(!idle.update(180009,1.005,false));assert(idle.update(180010,1.005,false));
 assert(!idle.update(180011,1.03,false)); // Real turn wakes, sensor jitter does not.
 assert(!idle.update(360010,1.03,false));assert(idle.update(360011,1.03,false));
 assert(!idle.update(360012,1.03,true)); // Any held or pressed key keeps it awake.
 for(unsigned t=0;t<400000;t+=1000)assert(!idle.update(360012+t,1.03,true));
 idle.wake(0xffffff00u,0);assert(!idle.update(0xffffff00u+179999u,0,false));assert(idle.update(0xffffff00u+180000u,0,false));
 idle.wake(100,0);idle.sleep();assert(idle.update(101,0,false));assert(!idle.update(102,.03f,false));
 idle.wake(100,0);assert(!idle.update(179000,std::numeric_limits<float>::quiet_NaN(),false));assert(idle.update(180100,0,false));
}
