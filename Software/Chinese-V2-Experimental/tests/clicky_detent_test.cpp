#include "../MacroPad/menu_step.h"
#include <cassert>

int main() {
  const float step = 1.0f;
  MenuStep wheel;
  wheel.begin(0.0f, step);
  // 小幅抖动和未越过格位门槛时不能向电脑发滚动。
  assert(wheel.update(0.49f) == 0);
  assert(wheel.update(0.79f) == 0);
  assert(wheel.update(0.81f) == 1);
  assert(wheel.center == 1.0f);
  // 刚过边界后向后回弹 0.2 格：不应发相反方向。
  assert(wheel.update(0.61f) == 0);
  assert(wheel.update(0.80f) == 0);
  assert(wheel.update(1.79f) == 0);
  assert(wheel.update(1.81f) == 1);
  // 真正转回一格时，允许正常的逆向滚动。
  assert(wheel.update(1.19f) == -1);
  assert(wheel.update(0.19f) == -1);
  assert(wheel.update(0.19f) == 0);

  // 从非零角度开始，单次最多发一个跨格事件，后续循环补齐高速转动。
  wheel.begin(-8.0f, step);
  assert(wheel.update(-5.1f) == 1);
  assert(wheel.update(-5.1f) == 1);
  assert(wheel.update(-5.1f) == 1);
  assert(wheel.update(-5.1f) == 0);
  return 0;
}
